# Run with Windows PowerShell 5.1 or pwsh. All IO is isolated in a temporary game.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../tools/update.ps1') -LibraryOnly
Add-Type -AssemblyName System.IO.Compression.FileSystem
$script:ManifestFileId = 'fixture_manifest_123'
$script:NetworkCalls = 0
$script:Running = $false
$script:FailDestination = ''
$script:RealCopy = (Get-Item Function:\Copy-UpdateFile).ScriptBlock
$root = Join-Path ([IO.Path]::GetTempPath()) ('dyniamod-tests-' + [Guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($root)

function Assert($Condition, [string]$Message)
{
	if (-not $Condition) { throw "FAIL: $Message" }
}

function Test-GameRunning { return $script:Running }

function Receive-UpdateFile([string]$Url, [string]$Destination)
{
	$script:NetworkCalls++
	if ($script:NetworkError) { throw 'Simulated network failure' }
	$source = if ($Url.Contains('id=fixture_manifest_123')) { $script:FixtureManifest } else { $script:FixtureZip }
	[IO.File]::Copy($source, $Destination, $true)
	if ($script:StartDuringDownload -and $source -eq $script:FixtureZip) { $script:Running = $true }
}

function Copy-UpdateFile([string]$Source, [string]$Destination)
{
	if ($script:FailDestination -eq $Destination) {
		$script:FailDestination = ''
		# Simulate a failure after a partially written destination.
		[IO.File]::WriteAllText($Destination, 'partial write')
		throw 'Simulated disk failure'
	}
	& $script:RealCopy $Source $Destination
}

function New-Game([string]$Name)
{
	$game = Join-Path $root $Name
	$null = [IO.Directory]::CreateDirectory($game)
	[IO.File]::WriteAllText((Join-Path $game 'DyniaMod.exe'), 'original exe')
	[IO.File]::WriteAllText((Join-Path $game 'config.ini'), 'player config')
	[IO.File]::WriteAllText((Join-Path $game 'th4sp_0.ssv'), 'player save')
	$null = [IO.Directory]::CreateDirectory((Join-Path $game 'th4_save_mp'))
	[IO.File]::WriteAllText((Join-Path $game 'th4_save_mp/th4mp_0.msv'), 'multiplayer save')
	return $game
}

function Set-Release([string]$Id, [hashtable]$Payload)
{
	$script:FixtureZip = Join-Path $root ([Guid]::NewGuid().ToString('N') + '.zip')
	$zip = [IO.Compression.ZipFile]::Open($script:FixtureZip, [IO.Compression.ZipArchiveMode]::Create)
	$files = @()
	try {
		foreach ($path in ($Payload.Keys | Sort-Object)) {
			$bytes = [Text.Encoding]::UTF8.GetBytes($Payload[$path])
			$entry = $zip.CreateEntry($path)
			$stream = $entry.Open()
			try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
			$sha = [Security.Cryptography.SHA256]::Create()
			try { $hash = [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant() }
			finally { $sha.Dispose() }
			$files += @{path = $path; sha256 = $hash; size = $bytes.Length}
		}
	} finally { $zip.Dispose() }
	$script:Release = @{releaseId = $Id; fileId = 'fixture_zip_123'; size = (Get-Item $script:FixtureZip).Length;
		sha256 = (Get-FileHash $script:FixtureZip -Algorithm SHA256).Hash.ToLowerInvariant(); files = $files}
	$script:FixtureManifest = Join-Path $root 'latest.json'
	Save-Manifest
}

function Save-Manifest
{
	@{schema = 1; app = 'DyniaMod'; latest = $script:Release} | ConvertTo-Json -Depth 10 |
		Set-Content -LiteralPath $script:FixtureManifest -Encoding UTF8
}

function Assert-PlayerData([string]$Game)
{
	Assert ((Get-Content -LiteralPath (Join-Path $Game 'config.ini') -Raw) -eq 'player config') 'config preserved'
	Assert ((Get-Content -LiteralPath (Join-Path $Game 'th4sp_0.ssv') -Raw) -eq 'player save') 'root save preserved'
	Assert ((Get-Content -LiteralPath (Join-Path $Game 'th4_save_mp/th4mp_0.msv') -Raw) -eq 'multiplayer save') 'MP save preserved'
}

try {
	# Download confirmation is parsed as data and remains pinned to Google's host and file ID.
	$page = '<form id="download-form" action="https://untrusted.example/download"><input name="id" value="fixture_zip_123"><input name="export" value="download"><input name="confirm" value="t"><input name="uuid" value="fixture-token"></form>'
	$url = Get-DriveConfirmationUrl (Get-DriveDownloadUrl 'fixture_zip_123') $page
	Assert ($url.StartsWith('https://drive.usercontent.google.com/download?')) 'confirmation host pinned'
	Assert ($url.Contains('id=fixture_zip_123') -and $url.Contains('confirm=t')) 'confirmation parameters preserved'
	$rejected = $false
	try { $null = Get-DriveConfirmationUrl (Get-DriveDownloadUrl 'different_file_123') $page } catch { $rejected = $true }
	Assert $rejected 'confirmation cannot switch file ID'
	Assert ($null -eq (Get-DriveConfirmationUrl (Get-DriveDownloadUrl 'fixture_zip_123') '{"schema":1}')) 'ordinary files need no confirmation'

	# First install, first-run hash marker, no-op, skipped releases, and one backup.
	$game = New-Game "game with spaces & apostrophe ' (test)"
	Set-Release '20261103T120000000000Z' @{'DyniaMod.exe' = 'release one'; 'assets/runtime.dat' = 'one'}
	Assert ((Invoke-GameUpdate $game) -eq 0) 'first install'
	Assert ((Get-Content -LiteralPath (Join-Path $game 'DyniaMod.exe') -Raw) -eq 'release one') 'exe replaced'
	Assert ((Get-Content -LiteralPath (Join-Path $game '.dyniamod-update-backup/files/DyniaMod.exe') -Raw) -eq 'original exe') 'backup original exe'
	$state = Get-Content -LiteralPath (Join-Path $game $script:StateName) -Raw | ConvertFrom-Json
	Assert ($state.sha256 -eq $script:Release.sha256) 'installed ZIP hash saved'
	$before = $script:NetworkCalls
	Assert ((Invoke-GameUpdate $game) -eq 0) 'already installed'
	Assert (($script:NetworkCalls - $before) -eq 1) 'no ZIP download when already installed'
	Set-Release '20261103T140000000000Z' @{'DyniaMod.exe' = 'release three'; 'assets/runtime.dat' = 'three'}
	Assert ((Invoke-GameUpdate $game) -eq 0) 'skip directly to release three'
	Assert ((Get-Content -LiteralPath (Join-Path $game '.dyniamod-update-backup/files/DyniaMod.exe') -Raw) -eq 'release one') 'only previous release backed up'
	Assert-PlayerData $game
	Set-Release '20261103T130000000000Z' @{'DyniaMod.exe' = 'older release'}
	Assert ((Invoke-GameUpdate $game) -eq 0) 'old manifest ignored'
	Assert ((Get-Content -LiteralPath (Join-Path $game 'DyniaMod.exe') -Raw) -eq 'release three') 'no downgrade'
	Set-Release '20261103T140000000000Z' @{'DyniaMod.exe' = 'reused release id'}
	Assert ((Invoke-GameUpdate $game) -eq 1) 'same ID with different ZIP rejected'

	# No releases, running game, unavailable network and broken ZIP all leave installation alone.
	$empty = New-Game 'empty-channel'
	$script:Release = $null
	Save-Manifest
	Assert ((Invoke-GameUpdate $empty) -eq 0) 'empty release channel'
	Assert (-not (Test-Path -LiteralPath (Join-Path $empty $script:StateName))) 'empty channel does not mark installed'
	$script:Running = $true
	$before = $script:NetworkCalls
	Assert ((Invoke-GameUpdate $empty) -eq 1) 'running game blocks update'
	Assert ($script:NetworkCalls -eq $before) 'running game blocks network'
	$script:Running = $false
	Set-Release '20261103T145000000000Z' @{'DyniaMod.exe' = 'game started during download'}
	$script:StartDuringDownload = $true
	Assert ((Invoke-GameUpdate $empty) -eq 1) 'game started during download blocks install'
	Assert ((Get-Content -LiteralPath (Join-Path $empty 'DyniaMod.exe') -Raw) -eq 'original exe') 'running game executable preserved'
	$script:StartDuringDownload = $false
	$script:Running = $false
	$script:NetworkError = $true
	Assert ((Invoke-GameUpdate $empty) -eq 1) 'network failure is an error'
	$script:NetworkError = $false
	[IO.File]::WriteAllText($script:FixtureManifest, '{"schema":1,"app":"DyniaMod"}')
	Assert ((Invoke-GameUpdate $empty) -eq 1) 'missing latest field is an error'
	Set-Release '20261103T150000000000Z' @{'DyniaMod.exe' = 'uninstalled'}
	[IO.File]::WriteAllText($script:FixtureZip, '<html>Google login or download quota</html>')
	Assert ((Invoke-GameUpdate $empty) -eq 1) 'corrupt ZIP rejected'
	Assert ((Get-Content -LiteralPath (Join-Path $empty 'DyniaMod.exe') -Raw) -eq 'original exe') 'bad download leaves exe alone'
	Assert (-not (Test-Path -LiteralPath (Join-Path $empty $script:StateName))) 'bad download does not mark installed'

	# State write fails after all program files were replaced; rollback restores files and state.
	$rollback = New-Game 'rollback'
	Set-Release '20261103T160000000000Z' @{'DyniaMod.exe' = 'broken install'; 'new-runtime.dat' = 'new'}
	$script:FailDestination = Join-Path $rollback $script:StateName
	Assert ((Invoke-GameUpdate $rollback) -eq 1) 'state write failure triggers rollback'
	Assert ((Get-Content -LiteralPath (Join-Path $rollback 'DyniaMod.exe') -Raw) -eq 'original exe') 'rollback restores executable'
	Assert (-not (Test-Path -LiteralPath (Join-Path $rollback 'new-runtime.dat'))) 'rollback removes newly added file'
	Assert (-not (Test-Path -LiteralPath (Join-Path $rollback $script:StateName))) 'rollback removes partially written state'
	Assert-PlayerData $rollback
	Assert ((Invoke-GameUpdate $rollback) -eq 0) 'retry after rollback works'
	$oldState = Get-Content -LiteralPath (Join-Path $rollback $script:StateName) -Raw
	Set-Release '20261103T170000000000Z' @{'DyniaMod.exe' = 'second failed install'}
	$script:FailDestination = Join-Path $rollback $script:StateName
	Assert ((Invoke-GameUpdate $rollback) -eq 1) 'rollback existing installation'
	Assert ((Get-Content -LiteralPath (Join-Path $rollback $script:StateName) -Raw) -eq $oldState) 'old state restored'
	Assert ((Get-Content -LiteralPath (Join-Path $rollback 'DyniaMod.exe') -Raw) -eq 'broken install') 'old release restored'

	# Manifest / archive disagreement, protected saves and path traversal are rejected.
	Set-Release '20261103T180000000000Z' @{'DyniaMod.exe' = 'not installed'; 'extra.dat' = 'extra'}
	$script:Release.files = @($script:Release.files | Where-Object { $_.path -ne 'extra.dat' })
	Save-Manifest
	Assert ((Invoke-GameUpdate $empty) -eq 1) 'undeclared ZIP file rejected'
	Set-Release '20261103T180001000000Z' @{'DyniaMod.exe' = 'not installed'}
	$script:Release.files[0].sha256 = ('0' * 64)
	Save-Manifest
	Assert ((Invoke-GameUpdate $empty) -eq 1) 'per-file hash mismatch rejected'
	foreach ($path in @('../outside.exe', '/absolute.exe', 'a\b', 'config.ini', 'th4sp_0.ssv', 'th4mp_0.msv', 'th4_save_mp/hero', 'diabdat.mpq', 'TH4data.mor', '.dyniamod-update.json', 'CON.txt', 'data/file:stream', 'data/space.')) {
		$rejected = $false
		try { $null = Get-SafeRelativePath $path } catch { $rejected = $true }
		Assert $rejected "protected path rejected: $path"
	}
	Set-Release '20261103T190000000000Z' @{'DyniaMod.exe' = 'not installed'; 'th4_save_mp/hero' = 'bad'}
	Assert ((Invoke-GameUpdate $empty) -eq 1) 'protected save in manifest blocks installation'
	Assert-PlayerData $empty

	# Competing updater does not enter the install path.
	$handle = [IO.File]::Open((Join-Path $empty '.dyniamod-update.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
	try { Assert ((Invoke-GameUpdate $empty) -eq 1) 'concurrent updater blocked' }
	finally { $handle.Dispose() }
	Assert (@(Get-ChildItem -LiteralPath $root -Recurse -Directory | Where-Object Name -Like '.dyniamod-update-work-*').Count -eq 0) 'temporary work cleaned'
	Write-Host 'PASS: updater installation, no-op, backups, rollback, integrity, player data, concurrency.'
} finally {
	Remove-Item -LiteralPath $root -Recurse -Force
}

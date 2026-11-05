param([switch]$LibraryOnly)

# Embedded in update.bat by release.py; compatible with Windows PowerShell 5.1.
$ErrorActionPreference = 'Stop'
$script:ManifestFileId = '__MANIFEST_FILE_ID__'
$script:StateName = '.dyniamod-update.json'
$script:BackupName = '.dyniamod-update-backup'

function Get-DriveDownloadUrl([string]$FileId)
{
	if ($FileId -cnotmatch '^[A-Za-z0-9_-]{10,200}$') { throw 'Nieprawidlowy identyfikator pliku Google Drive.' }
	return "https://drive.google.com/uc?export=download&id=$FileId"
}

function Get-DriveConfirmationUrl([string]$Url, [string]$Page)
{
	$form = [regex]::Match($Page, '(?is)<form\b[^>]*\bid="download-form"[^>]*>(.*?)</form>')
	if (-not $form.Success) { return $null }
	$id = [regex]::Match($Url, '^https://drive\.google\.com/uc\?export=download&id=([A-Za-z0-9_-]+)$')
	if (-not $id.Success) { throw 'Nieprawidlowe zrodlo pobierania Google Drive.' }
	$fields = @{}
	foreach ($inputTag in [regex]::Matches($form.Groups[1].Value, '(?is)<input\b[^>]*>')) {
		$name = [regex]::Match($inputTag.Value, '\bname="([^"]+)"')
		$value = [regex]::Match($inputTag.Value, '\bvalue="([^"]*)"')
		if ($name.Success -and $value.Success -and $name.Groups[1].Value -in @('id', 'export', 'confirm', 'uuid', 'resourcekey')) {
			$key = $name.Groups[1].Value
			if ($fields.ContainsKey($key)) { throw 'Powtorzony parametr potwierdzenia Google Drive.' }
			$fields[$key] = [Net.WebUtility]::HtmlDecode($value.Groups[1].Value)
		}
	}
	if ($fields['id'] -cne $id.Groups[1].Value -or $fields['export'] -cne 'download' -or
		$fields['confirm'] -cnotmatch '^[A-Za-z0-9_-]{1,200}$') { throw 'Nieprawidlowe potwierdzenie pobierania Google Drive.' }
	$query = @($fields.GetEnumerator() | ForEach-Object { [Uri]::EscapeDataString($_.Key) + '=' + [Uri]::EscapeDataString($_.Value) }) -join '&'
	# Never follow an action URL supplied by downloaded HTML.
	return ('https://drive.usercontent.google.com/download?' + $query)
}

function Receive-UpdateFile([string]$Url, [string]$Destination)
{
	[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
	$oldProgress = $ProgressPreference
	try {
		$ProgressPreference = 'SilentlyContinue'
		Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Destination -TimeoutSec 180 -MaximumRedirection 8 -SessionVariable driveSession -Headers @{'Cache-Control' = 'no-cache'}
		if ((Get-Item -LiteralPath $Destination).Length -lt 1048576) {
			$page = [IO.File]::ReadAllText($Destination)
			$confirmation = Get-DriveConfirmationUrl $Url $page
			if ($confirmation) {
				Invoke-WebRequest -UseBasicParsing -Uri $confirmation -OutFile $Destination -TimeoutSec 180 -MaximumRedirection 8 -WebSession $driveSession
			}
		}
	} finally { $ProgressPreference = $oldProgress }
}

function Test-GameRunning
{
	return @((Get-Process -Name 'DyniaMod' -ErrorAction SilentlyContinue)).Count -gt 0
}

function Assert-NoReparsePoint([string]$Path)
{
	$current = [IO.Path]::GetFullPath($Path)
	while ($current) {
		if ([IO.File]::Exists($current) -or [IO.Directory]::Exists($current)) {
			if (([IO.File]::GetAttributes($current) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
				throw "Sciezka prowadzi przez dowiazanie: $current"
			}
		}
		$parent = [IO.Path]::GetDirectoryName($current)
		if ($parent -eq $current) { break }
		$current = $parent
	}
}

function Get-SafeRelativePath([string]$Path)
{
	# Reject Windows path aliases, traversal, ADS streams and player-owned files.
	if ([string]::IsNullOrWhiteSpace($Path) -or $Path.Length -gt 200 -or $Path.Contains('\') -or $Path -match '[:<>"|?*\x00-\x1f]' -or $Path.StartsWith('/')) {
		throw "Niedozwolona sciezka w aktualizacji: $Path"
	}
	$parts = $Path.Split('/')
	foreach ($part in $parts) {
		if (-not $part -or $part -in @('.', '..') -or $part -match '[. ]$' -or $part -match '^(?i:CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\.|$)') {
			throw "Niedozwolona sciezka w aktualizacji: $Path"
		}
	}
	if ($parts[0] -match '^(?i:save.*|.*_save.*|characters?|players?|screenshots?|config|settings|\.dyniamod.*|updates?)$' -or
		$Path -match '(?i)\.(ini|cfg|sv|hsv|ssv|msv|csv|ksv|osv|drv|dsv|bak|key|mpq|mor)$') {
		throw "Aktualizacja probuje zastapic dane gracza lub archiwum gry: $Path"
	}
	return $Path
}

function Assert-FileHash([string]$Path, [string]$Hash, [long]$Size)
{
	if (-not [IO.File]::Exists($Path) -or (Get-Item -LiteralPath $Path).Length -ne $Size -or
		(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $Hash) {
		throw "Plik jest niekompletny lub ma nieprawidlowa sume SHA-256: $Path"
	}
}

function Get-ValidatedRelease($Manifest)
{
	if ($null -eq $Manifest -or $Manifest.schema -ne 1 -or $Manifest.app -cne 'DyniaMod' -or
		-not ($Manifest.PSObject.Properties.Name -contains 'latest')) { throw 'Nieobslugiwany plik latest.json.' }
	if ($null -eq $Manifest.latest) { return $null }
	$r = $Manifest.latest
	if ($r.releaseId -cnotmatch '^\d{8}T\d{12}Z$' -or $r.sha256 -cnotmatch '^[a-f0-9]{64}$' -or
		$r.size -isnot [ValueType] -or $r.size -lt 1 -or $r.size -gt 1073741824) { throw 'Nieprawidlowy opis wydania.' }
	$null = Get-DriveDownloadUrl $r.fileId
	$files = @($r.files)
	if ($files.Count -lt 1 -or $files.Count -gt 2000) { throw 'Nieprawidlowa lista plikow wydania.' }
	$seen = @{}
	[long]$total = 0
	foreach ($file in $files) {
		$path = Get-SafeRelativePath $file.path
		if ($seen.ContainsKey($path) -or $file.sha256 -cnotmatch '^[a-f0-9]{64}$' -or
			$file.size -isnot [ValueType] -or $file.size -lt 0 -or $file.size -gt 1073741824) { throw 'Nieprawidlowy plik w opisie wydania.' }
		$seen[$path] = $true
		$total += $file.size
	}
	if (-not $seen.ContainsKey('DyniaMod.exe') -or $total -gt 2147483648) { throw 'Brak DyniaMod.exe lub zbyt duza paczka.' }
	return $r
}

function Expand-VerifiedUpdate([string]$ZipPath, [string]$Stage, $Release)
{
	Add-Type -AssemblyName System.IO.Compression.FileSystem
	$expected = @{}
	foreach ($file in $Release.files) { $expected[$file.path] = $file }
	$archive = [IO.Compression.ZipFile]::OpenRead($ZipPath)
	try {
		$seen = @{}
		foreach ($entry in $archive.Entries) {
			$path = Get-SafeRelativePath $entry.FullName
			# Packages contain only files, with no extra directory entries.
			if ($seen.ContainsKey($path) -or -not $expected.ContainsKey($path) -or $entry.Length -ne $expected[$path].size -or
				(($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) { throw "Nieprawidlowa zawartosc ZIP: $path" }
			$seen[$path] = $true
		}
		if ($seen.Count -ne $expected.Count) { throw 'ZIP nie zawiera wszystkich plikow wydania.' }
		foreach ($entry in $archive.Entries) {
			$target = Join-Path $Stage $entry.FullName
			$null = [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
			[IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $false)
			Assert-FileHash $target $expected[$entry.FullName].sha256 $expected[$entry.FullName].size
		}
	} finally { $archive.Dispose() }
}

function Copy-UpdateFile([string]$Source, [string]$Destination)
{
	$null = [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Destination))
	[IO.File]::Copy($Source, $Destination, $true)
}

function Install-VerifiedUpdate([string]$GameDirectory, [string]$Stage, [string]$Work, $Release)
{
	$backup = Join-Path $GameDirectory $script:BackupName
	if (Test-Path -LiteralPath $backup) { Assert-NoReparsePoint $backup }
	$newBackup = Join-Path $Work 'backup'
	$null = [IO.Directory]::CreateDirectory($newBackup)
	$paths = @($Release.files | ForEach-Object { $_.path }) + @($script:StateName)
	$originals = @{}
	foreach ($path in $paths) {
		$target = Join-Path $GameDirectory $path
		Assert-NoReparsePoint $target
		if ([IO.Directory]::Exists($target)) { throw "Katalog blokuje plik aktualizacji: $path" }
		$originals[$path] = [IO.File]::Exists($target)
		if ($originals[$path]) { Copy-UpdateFile $target (Join-Path $newBackup ('files/' + $path)) }
	}
	$originals | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $newBackup 'restore.json') -Encoding UTF8
	# Rotate only after every original was copied; a failed preflight preserves the last backup.
	$previous = Join-Path $Work 'previous-backup'
	if (Test-Path -LiteralPath $backup) { [IO.Directory]::Move($backup, $previous) }
	try { [IO.Directory]::Move($newBackup, $backup) }
	catch {
		if (Test-Path -LiteralPath $previous) { [IO.Directory]::Move($previous, $backup) }
		throw
	}
	$touched = New-Object 'System.Collections.Generic.List[string]'
	try {
		# A game started while downloading must also block installation.
		if (Test-GameRunning) { throw 'Zamknij DyniaMod.exe i uruchom aktualizator ponownie.' }
		foreach ($file in $Release.files) {
			$target = Join-Path $GameDirectory $file.path
			Assert-NoReparsePoint $target
			$touched.Add($file.path)
			Copy-UpdateFile (Join-Path $Stage $file.path) $target
		}
		# State is committed last and included in rollback.
		$stateTemp = Join-Path $Work 'installed.json'
		@{schema = 1; releaseId = $Release.releaseId; sha256 = $Release.sha256; installedUtc = [DateTime]::UtcNow.ToString('o')} |
			ConvertTo-Json | Set-Content -LiteralPath $stateTemp -Encoding UTF8
		$touched.Add($script:StateName)
		Copy-UpdateFile $stateTemp (Join-Path $GameDirectory $script:StateName)
	} catch {
		$reason = $_.Exception.Message
		$restoreErrors = @()
		foreach ($path in $touched) {
			try {
				$target = Join-Path $GameDirectory $path
				if ($originals[$path]) { Copy-UpdateFile (Join-Path $backup ('files/' + $path)) $target }
				elseif ([IO.File]::Exists($target)) { [IO.File]::Delete($target) }
			} catch { $restoreErrors += $path }
		}
		if ($restoreErrors.Count) { throw "Blad instalacji: $reason Kopia w $backup. Nie udalo sie odtworzyc: $($restoreErrors -join ', ')." }
		throw "Instalacja przerwana; przywrocono poprzednie pliki. $reason"
	}
}

function Invoke-GameUpdate([string]$GameDirectory)
{
	$work = $null
	$lock = $null
	try {
		$GameDirectory = [IO.Path]::GetFullPath($GameDirectory)
		Assert-NoReparsePoint $GameDirectory
		if (-not [IO.File]::Exists((Join-Path $GameDirectory 'DyniaMod.exe'))) { throw 'Umiesc update.bat w katalogu zawierajacym DyniaMod.exe.' }
		if (Test-GameRunning) { throw 'Gra jest uruchomiona. Zamknij DyniaMod.exe i uruchom aktualizator ponownie.' }
		$lockPath = Join-Path $GameDirectory '.dyniamod-update.lock'
		Assert-NoReparsePoint $lockPath
		$lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
		# Work stays on the game volume so backup directory moves are atomic.
		$work = Join-Path $GameDirectory ('.dyniamod-update-work-' + [Guid]::NewGuid().ToString('N'))
		$null = [IO.Directory]::CreateDirectory($work)
		Write-Host 'Sprawdzanie aktualizacji DyniaMod...'
		$manifestPath = Join-Path $work 'latest.json'
		Receive-UpdateFile (Get-DriveDownloadUrl $script:ManifestFileId) $manifestPath
		if ((Get-Item -LiteralPath $manifestPath).Length -gt 2097152) { throw 'Nieprawidlowy rozmiar latest.json.' }
		$release = Get-ValidatedRelease (Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json)
		if ($null -eq $release) { Write-Host 'Nie ma zadnych aktualizacji.'; return 0 }
		$statePath = Join-Path $GameDirectory $script:StateName
		Assert-NoReparsePoint $statePath
		if ([IO.File]::Exists($statePath)) {
			$state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
			if ($state.schema -ne 1 -or $state.releaseId -cnotmatch '^\d{8}T\d{12}Z$' -or $state.sha256 -cnotmatch '^[a-f0-9]{64}$') { throw 'Nieprawidlowy lokalny plik stanu aktualizacji.' }
			if ($state.sha256 -eq $release.sha256 -or [string]::CompareOrdinal($state.releaseId, $release.releaseId) -gt 0) {
				Write-Host 'Nie ma zadnych aktualizacji.'; return 0
			}
			if ($state.releaseId -eq $release.releaseId) { throw 'Identyfikator wydania zostal ponownie uzyty z innym ZIP-em.' }
		}
		Write-Host "Pobieranie wydania $($release.releaseId)..."
		$zipPath = Join-Path $work 'update.zip'
		Receive-UpdateFile (Get-DriveDownloadUrl $release.fileId) $zipPath
		Assert-FileHash $zipPath $release.sha256 $release.size
		$stage = Join-Path $work 'stage'
		$null = [IO.Directory]::CreateDirectory($stage)
		Expand-VerifiedUpdate $zipPath $stage $release
		Install-VerifiedUpdate $GameDirectory $stage $work $release
		Write-Host "Zainstalowano aktualizacje $($release.releaseId). Kopia poprzednich plikow: $script:BackupName"
		return 0
	} catch {
		Write-Host "Blad aktualizacji: $($_.Exception.Message)" -ForegroundColor Red
		return 1
	} finally {
		if ($work -and [IO.Directory]::Exists($work)) {
			try { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction Stop }
			catch { Write-Host "Pozostal katalog roboczy: $work" }
		}
		if ($lock) { $lock.Dispose() }
	}
}

if (-not $LibraryOnly) {
	if ($PSVersionTable.PSVersion -lt [version]'5.1' -or [Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
		Write-Host 'Aktualizator wymaga Windows 11 i Windows PowerShell 5.1.'
		exit 1
	}
	exit (Invoke-GameUpdate ([IO.Path]::GetDirectoryName($env:DYNIA_UPDATE_SELF)))
}

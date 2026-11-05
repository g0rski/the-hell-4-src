# Aktualizacje DyniaMod na Windows 11

Skopiuj sam `update.bat` do katalogu zawierającego `DyniaMod.exe` i uruchom
dwuklikiem. Nie potrzeba Pythona, konta Google ani dodatkowego `.ps1`.
Aktualizator używa Windows PowerShell 5.1. Katalog gry musi pozwalać na zapis.
Jeżeli proces `DyniaMod` działa, wyświetla komunikat i kończy pracę.

Nowe wydanie jest pobierane i instalowane automatycznie. Jeśli nie ma nowego
wydania, pojawia się `Nie ma zadnych aktualizacji.`. Błąd sieci lub uszkodzona
paczka jest zgłaszana jako błąd. Okno czeka na klawisz, żeby można przeczytać wynik.

Publiczny katalog:
<https://drive.google.com/drive/folders/1jiCgb1xItTyN4jet1HgG5vo07gF02wdh>.
Początkowy manifest zawiera `latest: null`, czyli brak pierwszego wydania.

## Stan i kopia zapasowa

Wersja gry `0.774.1` nie zmienia się. Wydania identyfikuje osobny czas UTC
z mikrosekundami (np. `20261103T120000000000Z`) i SHA-256 całego ZIP-a.
Po instalacji powstaje `.dyniamod-update.json`. Nie usuwaj go: bez tego pliku
aktualizator ponownie zainstaluje najnowszą paczkę. Starszy manifest nie cofa gry.

Zastępowane pliki trafiają do `.dyniamod-update-backup/files/`; zachowana jest
jedna poprzednia kopia. `restore.json` w katalogu kopii wskazuje, które pliki
istniały wcześniej. Po błędzie instalacji aktualizator odtwarza poprzednie pliki
i stan oraz usuwa nowo dodane pliki. Jeśli odtworzenie także zawiedzie,
wyświetla lokalizację kopii i pliki wymagające ręcznej naprawy.
Przerwanie zasilania lub zabicie aktualizatora może wymagać ręcznego odtworzenia.

Aby ręcznie cofnąć ostatnią instalację, zamknij grę, skopiuj zawartość `files/`
z kopii do katalogu gry i usuń pliki oznaczone `false` w `restore.json`
(wcześniej nie istniały), w tym lokalny stan, jeśli wcześniej go nie było.
Następne uruchomienie aktualizatora ponownie zainstaluje aktualne publiczne wydanie.

Paczki nie mogą podmieniać konfiguracji, katalogów `th4_save_*`, archiwów postaci
`.ssv`, `.msv`, `.csv`, `.ksv`, `.osv`, `.dsv`, kopii `.bak` ani danych MPQ/MOR.
Pliki niewymienione w paczce pozostają w instalacji. Aktualizator nie uruchamia
skryptów z paczki. Własny BAT podmienia po wczytaniu całego programu do pamięci.

## Następne wydanie

Wywołaj `$dyniamod-release` i opisz zmianę w grze. Skill znajduje się
w [SKILL.md](../.agents/skills/dyniamod-release/SKILL.md) i jest zainstalowany
w `~/.codex/skills/dyniamod-release`. Wykonuje zmianę, buduje Release i Debug,
sprawdza wynik i publikuje ZIP przez Google Drive MCP.

Na obecnej maszynie używaj istniejącego Microsoft MSVC v145 przez Wine
i SDK 10.0.22621.0:

```sh
python3 tools/build_windows.py --check-tools
python3 tools/build_windows.py --clean
python3 tools/build_windows.py --debug --clean
```

Śledzony skrypt w `tools/` zastępuje dawny skrypt z ignorowanego `build/`.
Przyrostowe sprawdzenia dat nie zastępują czystej kompilacji wydania.

`tools/release.py` generuje BAT z `tools/update.ps1` i konfiguracji
`tools/release-config.json`. ZIP zawiera pełny aktualny EXE, cztery pliki
uruchomieniowe z `lib/`, `README.md`, aktualizator i `extraFiles` z konfiguracji.
Przykład mapowania zasobu: `{"source": "res/example.dat", "path": "example.dat"}`.
Lista zasobów jest kumulatywna względem bazowej instalacji, aby gracz mógł
pomijać wydania. Usuwanie przestarzałych plików wymaga rozszerzenia protokołu.
Lokalne pliki do publikacji trafiają do ignorowanego `updates/`, odpowiadającego
`/agents/the-hell-4-src/updates/` na Drive. Skrypt sam nie wykonuje uploadów.

## Protokół publikacji

Manifest jest zwykłym plikiem `latest.json` o stałym ID
`1qJ0QDkri3IFei8l_SeBK1DRW_8IZ-4iB`. Aktualizuj jego treść w miejscu;
usunięcie i ponowne utworzenie zerwie działanie istniejących BAT-ów.
Struktura: `schema: 1`, `app: "DyniaMod"`, `latest: null` albo obiekt
z polami `releaseId`, `fileId` ZIP-a, `sha256`, `size` i tablicą `files`.
Każdy wpis `files` ma `path`, `sha256` i `size`.

ZIP nie ma katalogu nadrzędnego ani osobnych wpisów katalogów. Aktualizator
sprawdza hash ZIP-a i każdego pliku, odrzuca nieoczekiwane pliki, powtórzenia
ścieżek oraz wyjście poza katalog gry. SHA-256 sprawdza integralność, ale nie
jest podpisem wydawcy: konto Drive kontroluje zarówno manifest, jak i paczkę.

Najpierw opublikuj ZIP i sprawdź anonimowe pobranie oraz SHA-256. Dopiero potem
podmień manifest. Nigdy nie zmieniaj zawartości ZIP-a już opublikowanego wydania.
Nie publikuj manifestu, jeżeli kompilacja lub testy się nie powiodły.
Aktualizator obsługuje standardowe potwierdzenie pobierania Google Drive,
pozostając przy tym samym ID pliku i serwerze Google. Limity pobrań, wymagane
logowanie i inne strony zamiast paczki powodują błąd, bez zmiany instalacji.

Publiczny odczyt ma wyłącznie `updates`. Zobacz dokumentację Google:
[uprawnienia](https://developers.google.com/workspace/drive/api/guides/manage-sharing)
i [pobieranie plików](https://developers.google.com/workspace/drive/api/guides/manage-downloads).

## Testy narzędzi

```sh
python3 tests/test_release.py
pwsh -NoProfile -File tests/test_updater.ps1
```

Na Windows drugi test uruchamia się również przez
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/test_updater.ps1`.
Testy używają katalogów tymczasowych i zastąpionej funkcji pobierania;
nie podmieniają prawdziwej gry ani manifestu Drive. Na Windows 11 sprawdź też
uruchomienie BAT-a z docelowego katalogu gry.

Przy przygotowaniu narzędzi wykonano kompilację Release wszystkich 149 jednostek
C++ przez MSVC v145 wraz z shaderami, zasobami i linkowaniem. Sprawdzono PE x86
i zachowanie zasobu wersji `v0.774.1`. Testy instalacji, braku aktualizacji,
kopii zapasowej, przywracania po błędzie, integralności i ochrony danych przeszły
w PowerShell 7 na Linuksie. Odczyt pustego kanału i pobieranie z potwierdzeniem
Google sprawdzono anonimowo na rzeczywistym Drive. Obsługę podmiany samego BAT-a
sprawdzono przez `cmd.exe` w Wine z zastąpionym poleceniem PowerShell.
Nie wykonano pełnego uruchomienia na natywnym Windows 11 ani publikacji gry.

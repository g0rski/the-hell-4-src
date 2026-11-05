---
name: dyniamod-release
description: Wykonaj powierzoną zmianę w DyniaMod, zweryfikuj i skompiluj grę dla Windows 11 oraz opublikuj ZIP aktualizacji w publicznym katalogu updates na Google Drive. Używaj przy zleceniu nowego wydania tej gry; samo poprawianie narzędzi wydawniczych nie oznacza publikacji gry.
---

# Wydawanie DyniaMod

Pracuj w repozytorium `the-hell-4-src`, obecnie
`/home/dynia/codeman-cases/the-hell-4-src`. Wykonaj powierzoną zmianę,
sprawdź ją, zbuduj grę i opublikuj aktualizację. Żądanie wydania obejmuje upload
i publiczny odczyt w opisanym katalogu; nie pytaj ponownie o zgodę na te czynności.

## Stałe ustalenia

- Cel: `DyniaMod.exe`, Win32/x86, Windows 11. Przeczytaj `AGENTS.md`, `BUILD.md`
  oraz [protokół aktualizacji](../../../doc/updating.md).
- **Nie zwiększaj wersji gry** w `tools/version.h`, zasobach ani innych
  miejscach. Wersja pozostaje `0.774.1`. Wydanie ma osobny identyfikator UTC
  i SHA-256 ZIP-a. Nie zmieniaj rewizji protokołu tylko z powodu wydania.
- ZIP zawiera program, biblioteki i wymagane zasoby; bez dużych archiwów danych,
  ustawień i zapisów. Nie publikuj Debug, PDB, źródeł ani sekretów jako aktualizacji.
- Aktualizator kończy pracę, jeśli gra działa, sam instaluje nową paczkę
  i zachowuje jedną poprzednią kopię zastępowanych plików.
- Konto Drive: `dzbanpandzban@gmail.com`. ID i alias są w
  `tools/release-config.json`. Publiczny katalog to wyłącznie
  `/agents/the-hell-4-src/updates/`. Pliki przygotuj lokalnie w `updates/`,
  zachowując względne ścieżki workspace na Drive.

## Zmiana i kompilacja

1. Sprawdź stan repozytorium i wykonaj powierzony zakres, zachowując istniejące
   zmiany użytkownika. Dobierz sprawdzenia do zachowania gry. Przy zmianach
   mechaniki uruchom istniejące testy współdzielenia przedmiotów, zapisu i mikstur
   zgodnie z `BUILD.md`. Oddziel testy automatyczne od prób wymagających danych
   gry, renderowania i multiplayera.
2. Na tej maszynie jest Microsoft MSVC v145 i Windows SDK przez Wine
   w `~/.cache/th4-windows-build`. Używaj śledzonego skryptu:

   ```sh
   python3 tools/build_windows.py --check-tools
   python3 tools/build_windows.py --clean
   python3 tools/build_windows.py --debug --clean
   ```

   Rejestruj wyniki i logi. Release powstaje w
   `build/windows-release/DyniaMod.exe`, Debug w podkatalogu `debug/`.
   Skrypt odtwarza konfigurację obecnego projektu; jeśli zmienisz opcje `.vcxproj`,
   uzgodnij je ze skryptem. `--clean` jest istotne: tryb przyrostowy nie śledzi
   pełnych zależności nagłówków. Na Windows użyj MSBuild v145 i czystego
   Release/Debug zgodnie z `BUILD.md`.
3. Wymagaj kodu wyjścia 0 i świeżego EXE obu konfiguracji. Sprawdź PE x86
   i zachowanie wersji gry. Testy przenośne nie zastępują kompilacji gry.
   Jeśli brakuje narzędzi, odtwórz je z dostępnej konfiguracji. Po błędzie
   zbadaj logi i napraw przyczynę w zakresie zadania. Jeśli to niemożliwe,
   podaj konkretną przeszkodę; nigdy nie publikuj starego EXE zamiast nowego.

## Paczka i publikacja

1. Przez Google Drive MCP sprawdź konto, folder i odczytaj aktualny `latest.json`
   o ID z konfiguracji. Zapisz go w `updates/previous-latest.json` do ewentualnego
   przywrócenia. Nie twórz nowego manifestu: istniejące BAT-y mają zapisane jego ID.
2. Dodatkowe pliki dopisz jako mapowania `source` / `path` do `extraFiles`
   w `tools/release-config.json`. Lista jest **kumulatywna** względem bazowej
   instalacji, ponieważ gracze mogą pomijać wydania. Nie dopisuj całego katalogu
   gry, konfiguracji, zapisów ani MPQ/MOR. Usuwanie plików/migracje wymagają
   dostosowania protokołu przed wydaniem; obecny aktualizator tylko zastępuje pliki.
3. Uruchom:

   ```sh
   python3 tools/release.py render-updater
   python3 tools/release.py package --executable build/windows-release/DyniaMod.exe
   ```

   Powstaje ZIP bez katalogu nadrzędnego i `*.release.json` w `updates/`.
   ID to UTC z mikrosekundami, niezależny od wersji gry. Sprawdź pliki i sumy.
   ID musi być większe od opublikowanego; nie używaj ponownie ID wydania.
   Po zmianie narzędzi uruchom `python3 tests/test_release.py` oraz
   `tests/test_updater.ps1` z dostępnym PowerShellem.
4. Wyślij ZIP przez MCP `gdrive.uploadFile` do `updates`, używając `localPath`
   wskazującego `updates/<nazwa>.zip`. Jeśli serwer nie widzi workspace, użyj
   obsługiwanego przesyłania binarnego, nie udawaj sukcesu. Po niejednoznacznym
   błędzie przed ponowieniem wyszukaj plik po nazwie i sumie, aby nie dublować uploadu.
5. Sprawdź publiczne prawo `anyone/reader` wyłącznie na folderze `updates`;
   w razie potrzeby ustaw je przez MCP. Nie publikuj rodziców folderu.
   Pobierz ZIP anonimowo i porównaj rozmiar i SHA-256 z lokalnym opisem.
   Strona logowania, limit pobrań lub strona potwierdzenia Google nie są ZIP-em.
   Standardowe potwierdzenie Google obsługuje `Receive-UpdateFile` z
   `tools/update.ps1` (załaduj z `-LibraryOnly`); nie pomijaj kontroli SHA-256.
6. Dopiero po weryfikacji ZIP-a utwórz manifest:

   ```sh
   python3 tools/release.py manifest --descriptor updates/<wydanie>.release.json \
     --file-id <ID_ZIP_NA_DRIVE> --current updates/previous-latest.json
   ```

   Ponownie odczytaj opublikowany manifest bezpośrednio przed podmianą.
   Jeśli zmienił się w trakcie pracy, uzgodnij kolejność i nie nadpisuj nowszego
   wydania. Wyślij `updates/latest.json` przez `gdrive.uploadFile` z **fileId
   istniejącego manifestu**, bez parametru folderu. To ostatni krok publikacji.
7. Sprawdź anonimowy odczyt manifestu oraz zgodność ID i SHA-256 ZIP-a.
   Po błędzie przywróć poprzedni manifest pod tym samym ID i sprawdź wynik.
   Po dwóch nieudanych próbach tego samego działania zbadaj przyczynę; zakończ
   z konkretną przeszkodą, jeśli nie możesz jej usunąć. Nie ponawiaj ślepo uploadów.

Po udanym wydaniu podaj zmianę, wyniki kompilacji/testów, ID wydania, SHA-256
i publiczny link do ZIP-a. Link do folderu nie zastępuje linku do paczki.
Wskaż wymagane, niewykonane próby gameplayowe.

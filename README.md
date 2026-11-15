# Monte Carlo sekwencyjnie i w OpenMP

Programy `mc_seq` i `mc_omp` liczą całki metodą Monte Carlo. Pierwszy działa
sekwencyjnie, drugi korzysta z OpenMP. Oba obsługują warstwowanie w 1D,
zatrzymywanie według dokładności oraz pomiary kosztu generatora i funkcji.

## Wspólne parametry

Wszystkie ustawienia eksperymentu znajdują się w `include/mc/config.h`.
Stała `mc::CONFIG` przekazuje je do obu wersji. Kolejne wersje programu powinny
korzystać z tych samych ustawień.

- `FUNCTION` wybiera `Sin`, `Gauss1d`, `Peak` lub `GaussNd`.
- `SAMPLES` określa stałą liczbę próbek, domyślnie milion.
- `TARGET_ABS = std::nullopt` włącza stały budżet próbek. Wartość np. `0.001`
  włącza obliczenia do zadanej dokładności, z limitem `MAX_SAMPLES`.
- `SEED` określa ziarno losowania, a `BATCH` liczbę próbek na rundę MC.
- `THREADS` określa liczbę wątków OpenMP, domyślnie 4. Wartość 0 korzysta
  z ustawienia środowiska OpenMP. `mc_seq` zawsze pracuje jednym wątkiem.
- `STRATA = 0` oznacza zwykłe MC, a `STRATA = 64` włącza warstwowanie w 1D.
- `MODE` wybiera `Full`, `RngOnly` lub `FuncOnly`.
- `CSV_PATH` określa plik wyników. Pusty tekst oznacza tylko wypisanie na ekran.

`parameters.h` zawiera typ `Config` ze stałymi wartościami domyślnymi.
Ustawienia eksperymentu zmieniaj w `config.h`. Testy tworzą własne konfiguracje,
więc wybór innej funkcji lub celu w `config.h` nie zmienia przypadków testowych.

Po zmianie ustawień przebuduj program. Uruchomienie nie wymaga argumentów.

## Budowanie i uruchamianie

Potrzebne są CMake 3.16 lub nowsze, kompilator C++17 i OpenMP. Na macOS
zainstaluj `cmake` i `libomp` przez Homebrew. Na Linuksie można użyć GCC.
Python 3 jest potrzebny tylko do testów uruchomienia programu.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/mc_seq
./build/mc_omp
ctest --test-dir build --output-on-failure
```

Random123 1.14.0 znajduje się wraz z licencją w `external/Random123`.
Budowanie nie pobiera zależności z internetu.

## VS Code

Otwórz cały katalog projektu. Po skonfigurowaniu przez CMake edytor korzysta
z `build/compile_commands.json`, który zawiera ścieżki nagłówków i ustawienia
kompilatora. Konfiguracja C/C++ znajduje się w `.vscode/c_cpp_properties.json`.
Po jej dodaniu odśwież okno poleceniem `Developer: Reload Window`.

## Obliczenia i wyniki

Philox wyznacza losowania z ziarna, indeksu próbki i numeru współrzędnej.
Wątki dostają rozłączne indeksy przez `schedule(static)`. Liczą lokalne statystyki
Welforda, które program łączy wzorem Chana. Zmiana liczby wątków zachowuje
punkty losowe. Kolejność sumowania może zmienić ostatnie cyfry wyniku.
`mc_seq` liczy te same partie zwykłą pętlą. Nie wywołuje ani nie dołącza OpenMP.

Funkcje `Gauss1d` i `Peak` liczymy na przedziale od 0 do 1, `Sin` od 0 do π,
a `GaussNd` na [0,1]⁴. Warstwowanie dzieli przedział 1D na 64 równe części.
Wariancja estymatora uwzględnia osobno wariancję i liczbę próbek każdej warstwy.

Wspólny moduł `monte_carlo.h` i `src/monte_carlo.cpp` oblicza estymatę,
przedział ufności i decyzję o zatrzymaniu. Typy wyników znajdują się w `result.h`.
Ta część nie wymaga OpenMP. Wersja OpenMP dzieli próbki i zbiera statystyki.

Program sprawdza cel po każdej rundzie. Kończy po co najmniej 4096 próbkach,
gdy `1.96 * std_err <= target_abs`, albo po osiągnięciu `max_samples`.
Nie przekracza limitu. `reached=1` oznacza osiągnięcie zadanej półszerokości
przybliżonego przedziału 95%. Nie jest gwarancją rzeczywistego błędu całki.

Wynik zawiera nagłówek CSV i jeden wiersz danych. Niepusty `CSV_PATH` dodatkowo
włącza dopisywanie do pliku. `t_total` mierzy czas obliczeń wraz z synchronizacją,
a `t_reduce` czas łączenia statystyk i sprawdzenia celu. Zapis CSV jest poza pomiarem.
Kolumna `backend` rozróżnia `seq` i `omp`; dla `seq` pole `workers` wynosi 1.

Tryby `RngOnly` i `FuncOnly` mają osobne pętle pomiarowe dla obu wersji.
Operacje z `benchmark_samples.h` są wspólne. Pierwszy tryb mierzy generator,
drugi funkcję na deterministycznych punktach.
Operacja jest wybierana przed pętlą próbek. Pomiary sumują wyniki do `checksum`,
bez statystyk Welforda i przedziałów ufności. Pola estymaty w CSV pozostają puste.

Pomiary wymagają `TARGET_ABS = std::nullopt` i `STRATA = 0`. Liczą wszystkie
`SAMPLES` w jednym przebiegu, bez rund `BATCH`. `t_reduce` mierzy połączenie
sum częściowych. W pomiarze sekwencyjnym nie ma końcowego scalania, więc
`t_reduce` wynosi 0. Czasów tych pomiarów nie sumujemy jako części czasu pełnego MC.
Ich definicja różni się od wcześniejszych pomiarów, które liczyły też statystyki.

## Aktualne pomiary

Wyniki bieżącej wersji znajdują się w `results/2026-11-15`. Zawierają 3560 przypadków
OpenMP i 3145 sekwencyjnych, surowe CSV, podsumowania, dziennik testów oraz krótką
notatkę na konsultacje. `metadata.json` zapisuje sumy kontrolne źródeł i programów.
Stare wyniki zostały zastąpione.

Serie pomiarów korzystają z zapisanych w kodzie zestawów `Config` w
`bench/experiments.cpp`. Każdy przypadek uruchamia aktualne funkcje obliczeniowe
w osobnym procesie. `mc_seq` i `mc_omp` nadal uruchamiamy bez argumentów.

Powtórzenie pełnej serii do nowego katalogu:

```sh
python3 bench/run_experiments.py --output /tmp/o06-powtorka
```

Skrypt przebudowuje kod w trybie Release, uruchamia testy i sprawdza zgodność
wyników obu wersji. Katalog wyjściowy musi być nowy. Czasy `t_total` nie obejmują
startu procesu, budowania listy przypadków ani zapisu CSV. `wall_seconds` obejmuje
całe uruchomienie procesu. Pomiary wykonywane są kolejno, w losowej kolejności
przypadków. Wykryta zmiana źródeł w trakcie serii przerywa zatwierdzenie wyników.

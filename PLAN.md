# O06. Monte Carlo z kontrolą dokładności

Zespół tworzą Konrad i Kacper.

## Cel i zakres

Porównamy czas obliczania całek metodą Monte Carlo w wersjach OpenMP, MPI oraz CUDA lub OpenCL. Wersja sekwencyjna będzie punktem odniesienia. Kod piszemy w C++17 i budujemy przez CMake.

Główny wynik to czas potrzebny do osiągnięcia zadanej dokładności. Zbadamy też błąd, przedziały ufności, próbki na sekundę oraz koszt generatora, funkcji i agregacji.

Dla dwóch osób wymagane jest warstwowanie lub reguła zatrzymania według dokładności. W projekcie zachowujemy oba rozszerzenia. Warstwowanie ograniczamy do funkcji 1D i 64 równych przedziałów. Liczby próbek w warstwach mogą różnić się najwyżej o jeden.

## Wspólne ustawienia i obliczenia

Na obecnym etapie parametry pozostają stałymi w `include/mc/config.h`. Wszystkie wersje mają korzystać z tej samej konfiguracji. Po zmianie ustawień przebudowujemy program i uruchamiamy go bez argumentów.

Konfiguracja określa funkcję, budżet próbek, ziarno, wielkość partii i liczbę wątków. `TARGET_ABS = std::nullopt` oznacza stały budżet `SAMPLES`. Wartość liczbowa włącza zatrzymywanie według dokładności, z limitem `MAX_SAMPLES`. `STRATA` wybiera zwykłe losowanie lub 64 warstwy.

Philox4x32-10 z biblioteki Random123 wyznacza losowania z ziarna, globalnego indeksu próbki i numeru współrzędnej. Pracownicy dostają rozłączne indeksy. Dla tej samej konfiguracji wszystkie wersje mają generować te same punkty.

Statystyki liczymy algorytmem Welforda i łączymy wzorem Chana. Matematyka MC i reguła zatrzymania są wspólne. Backend dzieli pracę i zbiera statystyki.

Przyjmujemy przybliżony przedział ufności 95%. Program sprawdza cel po każdej rundzie i po co najmniej 4096 próbkach kończy obliczenia, gdy połowa szerokości przedziału nie przekracza `TARGET_ABS`. Nie przekracza limitu próbek. Dla warstwowania korzysta z wariancji estymatora warstwowego.

Wynik CSV zawiera parametry, estymatę, błąd kontrolny, przedział ufności, liczbę próbek, status celu i czasy. Osobne tryby pomiaru generatora i funkcji zwracają czas oraz sumę kontrolną.

## Funkcje testowe

| Funkcja | Obszar | Całka kontrolna |
|---|---|---|
| `sin`, sin(x) | od 0 do π | 2 |
| `gauss1d`, exp(-x²) | od 0 do 1 | około 0,7468241328 |
| `peak`, 1 / [1 + 25x²] | od 0 do 1 | atan(5) / 5 |
| `gaussNd`, exp(-Σxᵢ²), wymiar 4 | [0,1]⁴ | całka `gauss1d` do potęgi 4 |

## Stan prac i podział zadań

Gotowe są wspólne moduły, wersja sekwencyjna `mc_seq` i wersja OpenMP `mc_omp`. Obie obsługują warstwowanie, regułę zatrzymania, pomiary kosztu i ten sam format CSV. Wersja sekwencyjna działa bez OpenMP i stanowi punkt odniesienia. Kod ma testy numeryczne, testy uruchomienia i porównanie obu wersji.

Konrad odpowiada za wspólną matematykę, wersję sekwencyjną, OpenMP oraz analizę wyników i wykresy.

Kacper odpowiada za MPI, wersję GPU, sprawdzenie dostępnego sprzętu i uruchamianie serii eksperymentów. Raport i interpretację wyników przygotujemy wspólnie.

Pozostała kolejność prac:

1. Dodać MPI z rozłącznymi zakresami próbek i scalaniem statystyk.
2. Dodać wersję GPU w CUDA albo OpenCL z równoległym generowaniem próbek, obliczaniem funkcji i redukcją.
3. Porównać wyniki wszystkich wersji dla tych samych parametrów, następnie wykonać końcowe eksperymenty i raport.

Preferujemy CUDA, jeśli będzie dostęp do maszyny NVIDIA. Możemy też spróbować środowiska GPU w Google Colab. Colab pozwala wykonywać polecenia systemowe, kompilować pliki `.cu` przez `nvcc` i uruchamiać programy CUDA C++. Dostępność GPU i narzędzi sprawdzimy przed rozpoczęciem prac. Alternatywą jest OpenCL na dostępnym, obsługiwanym urządzeniu. Przygotujemy jedną z tych dwóch wersji GPU.

## Testy i eksperymenty

Sprawdzimy zgodność estymat, wariancji i decyzji o zatrzymaniu między wersjami, z tolerancją na zaokrąglenia. Uwzględnimy niepełną ostatnią partię i wyczerpanie limitu próbek.

Eksperymenty obejmą:

- 10 tysięcy, 100 tysięcy i milion próbek oraz 100 ziaren do oceny błędu i pokrycia przedziałów;
- kilka liczb wątków i procesów oraz porównanie z wersją GPU i sekwencyjną;
- czas do dokładności 0,01, 0,001 i 0,0001, przy zwykłym i warstwowym MC;
- wpływ wielkości partii oraz osobne pomiary kosztu generatora, funkcji i agregacji.

Do porównań dokładności zachowamy ten sam budżet losowań. Pomiary czasu powtórzymy co najmniej pięć razy i podamy mediany. GPU rozgrzejemy przed pomiarami.

Serie pomiarów korzystają ze wspólnych zestawów parametrów w `bench/experiments.cpp`. Skrypt `bench/run_experiments.py` przebudowuje kod, uruchamia testy i wykonuje przypadki w osobnych procesach. Aktualne wyniki sekwencyjne i OpenMP znajdują się w `results/2026-11-15`, wraz z sumami kontrolnymi źródeł. Obejmują 6705 uruchomień i zastępują wcześniejsze CSV. Po dodaniu MPI i wersji GPU rozszerzymy porównanie.

Osobnych czasów generatora i funkcji nie sumujemy jako części czasu pełnego MC. Opiszemy narzut pomiarów i sprawdzimy pokrycie przedziałów przy zatrzymywaniu. Zadana szerokość przedziału nie gwarantuje rzeczywistego błędu.

Oddamy kod, instrukcję uruchomienia, surowe CSV, wykresy i raport porównujący dokładność oraz wydajność metod.

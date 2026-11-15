# O06. Monte Carlo z kontrolą dokładności
## Cel i zakres

Porównamy sekwencyjne i równoległe obliczanie całek metodą Monte Carlo. Program zakończy obliczenia po osiągnięciu zadanej dokładności albo limitu próbek. Sprawdzimy, jak próbkowanie warstwowe wpływa na błąd i czas obliczeń.

Przygotujemy cztery wersje programu w C++17, budowane przez CMake:

- `mc_seq`, wersja sekwencyjna i punkt odniesienia,
- `mc_omp`, wersja OpenMP,
- `mc_mpi`, wersja MPI,
- `mc_cuda`, wersja CUDA.

Próbkowanie warstwowe ograniczymy do funkcji jednowymiarowych. Użyjemy 64 równych przedziałów i jednakowej liczby próbek w każdym. Rezygnujemy z adaptacyjnego przydziału próbek i łączenia MPI z OpenMP.

## Wspólne zasady

Wszystkie wersje korzystają z tych samych funkcji, generatora, statystyk i formatu wyników.

Użyjemy generatora Philox4x32-10 z biblioteki Random123. Każdą liczbę losową wyznaczą ziarno, globalny indeks próbki i numer współrzędnej. Dzięki temu wersje programu dostaną te same punkty, a pracownicy nie będą powielać losowań.

Statystyki policzymy algorytmem Welforda, a wyniki częściowe połączymy wzorem Chana. Dla całki uwzględnimy objętość obszaru całkowania.

Przyjmiemy przybliżony przedział ufności 95%. Dokładność sprawdzimy po każdej rundzie 65 536 nowych próbek. Program zatrzyma się, gdy połowa szerokości przedziału będzie nie większa niż zadany błąd bezwzględny. Domyślny limit wyniesie 100 milionów próbek. Wynik wskaże, czy program osiągnął cel, czy wyczerpał limit.

Program obsłuży dwa tryby:

- stałą liczbę próbek przez `--samples`,
- obliczenia do zadanej dokładności przez `--target-abs` i `--max-samples`.

Opcja `--strata 64` włączy próbkowanie warstwowe. Jego przedział ufności policzymy z wariancji estymatora warstwowego.

Każde uruchomienie zapisze wiersz CSV z parametrami, estymatą, błędem względem znanej całki, przedziałem ufności, liczbą próbek, statusem zakończenia i czasem.

## Funkcje testowe

Użyjemy funkcji o znanych całkach:

| Funkcja | Obszar | Dokładna całka |
|---|---|---|
| `sin` | od 0 do π | 2 |
| `gauss1d`, exp(-x²) | od 0 do 1 | około 0,7468241328 |
| `peak`, 1 / [1 + 25x²] | od 0 do 1 | atan(5) / 5 |
| `gaussNd`, exp(-Σxᵢ²), wymiar 4 | [0,1]⁴ | całka `gauss1d` do potęgi 4 |

## Podział pracy i kolejność

Konrad przygotuje generator, statystyki, funkcje testowe, regułę zatrzymania oraz wersje sekwencyjną i OpenMP. Odpowiada też za analizę wyników i wykresy w Pythonie.

Kacper przygotuje CMake, obsługę argumentów, zapis CSV oraz wersje MPI i CUDA. Przygotuje wspólny kod próbkowania warstwowego i skrypty eksperymentów. Konrad wykorzysta ten kod w wersjach sekwencyjnej i OpenMP.

Prace wykonamy w następującej kolejności:

1. Uruchomimy wersję sekwencyjną ze stałą liczbą próbek i ustalimy wspólne interfejsy.
2. Dodamy OpenMP, MPI i CUDA. Porównamy wyniki dla tych samych próbek.
3. Dodamy zatrzymywanie według dokładności i próbkowanie warstwowe.
4. Wykonamy eksperymenty i wspólnie napiszemy raport.

W OpenMP zastosujemy statyczny podział pracy. W MPI proces główny zbierze i połączy statystyki oraz przekaże decyzję o zatrzymaniu. W CUDA bloki policzą statystyki częściowe, które połączy procesor.

Kacper sprawdzi dostęp do maszyny NVIDIA przed rozpoczęciem wersji CUDA.

## Testy i eksperymenty

Najpierw sprawdzimy generator, łączenie statystyk i zgodność czterech wersji z tolerancją na zaokrąglenia.

Następnie zbadamy:

- błąd i pokrycie przedziałów ufności dla 10 tysięcy, 100 tysięcy i miliona próbek, przy 100 ziarnach w wersji sekwencyjnej,
- przyspieszenie OpenMP i MPI dla 1, 2, 4 i 8 pracowników, w granicach dostępnego sprzętu, oraz wydajność CUDA,
- czas do dokładności 0,01, 0,001 i 0,0001 dla zwykłego i warstwowego Monte Carlo,
- koszt generowania liczb i obliczania funkcji przez osobne pomiary tych operacji.

Pomiary czasu powtórzymy pięć razy i podamy medianę. Przed pomiarem CUDA wykonamy rozgrzewkę. Osobne pomiary generatora i funkcji potraktujemy jako porównanie kosztów, ponieważ ich czasy nie muszą sumować się do czasu pełnego obliczenia.

Sprawdzimy pokrycie przedziałów także przy zatrzymywaniu według dokładności. Osiągnięcie zadanej szerokości przedziału nie gwarantuje, że rzeczywisty błąd będzie mniejszy od progu.

Oddamy kod, instrukcję uruchomienia, surowe wyniki CSV, skrypt generujący wykresy i raport porównujący dokładność oraz wydajność metod.

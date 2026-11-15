# Wyniki sekwencyjne i OpenMP

## Krótka wypowiedź na konsultacje

Mam wersję sekwencyjną i OpenMP ze wspólnym generatorem, statystykami i regułą zatrzymania. Sprawdziłem zgodność ich wyników dla wielu ziaren. Dla gauss1d i 10 milionów próbek osiem wątków dało 5,9 razy krótszy czas niż wersja sekwencyjna. Warstwowanie w 1D zmniejszyło RMSE około 30 do 65 razy przy tym samym budżecie próbek.

Wielkość partii wpływa na koszt synchronizacji i moment sprawdzenia celu. Duża partia może przyspieszyć długie obliczenie, ale wykonać za dużo próbek przed zatrzymaniem. Przedział 95% traktuję jako przybliżenie statystyczne, nie gwarancję błędu.

## Wyniki i krótki opis

Pomiary z 2026-11-15 obejmują 6705 uruchomień obecnego kodu, w tym 3560 OpenMP i 3145 sekwencyjnych. Sprzęt udostępnia 14 procesorów logicznych. Każdy przypadek działał w osobnym procesie, w kompilacji Release. Czasy dotyczą obliczeń i pomijają start procesu oraz zapis CSV. Nie izolowałem laptopa od innych aplikacji.

### Skalowanie liczby wątków

`gauss1d`, 10 milionów próbek, partia 1 048 576. Mediany siedmiu pomiarów.

| Wersja | Czas | Przyspieszenie względem sekwencyjnej |
|---|---:|---:|
| Sekwencyjna | 77,47 ms | 1,00× |
| OpenMP, 1 wątek | 78,64 ms | 0,99× |
| OpenMP, 4 wątki | 22,49 ms | 3,44× |
| OpenMP, 8 wątków | 13,09 ms | 5,92× |
| OpenMP, 14 wątków | 13,40 ms | 5,78× |

Przejście z 8 do 14 wątków zmieniło czas z 13,09 do 13,40 ms. Punktem odniesienia jest teraz osobna wersja sekwencyjna bez OpenMP.

### Dokładność i warstwowanie

Milion próbek i 100 ziaren na przypadek. RMSE to pierwiastek ze średniego kwadratu błędu względem znanej całki. Tabela pokazuje wersję sekwencyjną; wyniki porównano z OpenMP dla tych samych próbek.

| Funkcja | RMSE zwykłego MC | RMSE 64 warstw | Ile razy mniejszy błąd |
|---|---:|---:|---:|
| sin | 9,910e-04 | 3,312e-05 | 29,9 |
| gauss1d | 1,785e-04 | 2,743e-06 | 65,1 |
| peak | 2,550e-04 | 6,356e-06 | 40,1 |

Zwiększenie liczby próbek 100 razy zmniejszyło RMSE zwykłego MC około 9,1 do 11,2 razy. Wynik odpowiada zależności 1/√N.

### Czas do zadanej dokładności

`peak`, cel 0,0001, partia 65 536. OpenMP używa czterech wątków. Mediany pięciu pomiarów.

| Wersja i metoda | Liczba próbek | Czas |
|---|---:|---:|
| Sekwencyjna, zwykłe MC | 31 195 136 | 223,597 ms |
| Sekwencyjna, 64 warstwy | 65 536 | 0,482 ms |
| OpenMP, zwykłe MC | 31 195 136 | 77,170 ms |
| OpenMP, 64 warstwy | 65 536 | 0,209 ms |

Cel dotyczy połowy szerokości oszacowanego przedziału. Czasy poniżej milisekundy są orientacyjne.

### Wielkość partii

`peak`, 64 warstwy, cel 0,001 i cztery wątki OpenMP. Mediany pięciu pomiarów.

| Wielkość partii | Próbki do zatrzymania | Czas |
|---:|---:|---:|
| 4096 | 4096 | 0,080 ms |
| 65536 | 65536 | 0,214 ms |
| 1048576 | 1048576 | 2,077 ms |

Przy milionie próbek przedziały zawierały dokładną całkę w 93 do 98 procent uruchomień. Każdy przypadek obejmował 100 ziaren. `reached=1` oznacza spełnienie kryterium statystycznego, a nie gwarancję rzeczywistego błędu.

Osobne koszty generatora, funkcji i agregacji znajdują się w `summary.csv`. Pomiary generatora i funkcji sumują wyniki do checksum, bez liczenia wariancji. Ich czasów nie sumujemy jako części czasu pełnego MC.

# O06 – Monte Carlo z kontrolą dokładności – plan projektu

**Zespół:** A – Konrad, B – Kacper
**Zakres 2-osobowy:** klasyczne MC + **reguła kończenia oparta na dokładności** (wymagane) + **próbkowanie warstwowe** (rozszerzenie, mocno wzmacnia raport i odpowiada na pytanie „jak strategia próbkowania wpływa na czas do dokładności”).

---

## 1. Kluczowe decyzje techniczne (ustalić na starcie, oboje)

| Temat | Decyzja | Uzasadnienie |
|---|---|---|
| Język / build | C++17, CMake, osobne targety: `mc_seq`, `mc_omp`, `mc_mpi`, `mc_cuda` | Wspólny kod nagłówkowy dla CPU i GPU |
| Generator | **Philox4x32-10** (counter-based, własna implementacja ~50 linii, `__host__ __device__`) | Próbka o indeksie `i` = `philox(key=seed, counter=i)` → jednoznaczne mapowanie indeksu na stan, brak wspólnego stanu, ten sam wynik niezależnie od liczby wątków/procesów/GPU (świetne do walidacji) |
| Podział pracy | Partia `b` = próbki `[b·B, (b+1)·B)`; wątki/procesy/bloki dostają rozłączne zakresy partii | Spełnia wymóg rozłącznych strumieni |
| Statystyki | Struktura `Stats {n, mean, M2}` + aktualizacja Welforda + scalanie Chana (`merge(a,b)`) | Stabilna numerycznie wariancja, łatwa redukcja w OMP/MPI/CUDA |
| Przedział ufności | `mean ± z·s/√n`, z = 1.96 (95%), opcjonalnie 99% | Standard dla MC (CTW) |
| Wyjście programu | 1 wiersz CSV na uruchomienie (opis w §3) | Skrypty benchmarków i wykresy czytają jeden format |
| Wymiary | Funkcje 1D oraz d-wymiarowe (d = 1…8) | Pokazuje, że MC nie zależy od wymiaru, a warstwowanie tak |

**Funkcje testowe (znana całka):**

1. `sin` – ∫₀^π sin x dx = 2 (gładka, tania)
2. `gauss1d` – ∫₀¹ e^(−x²) dx = (√π/2)·erf(1) ≈ 0.746824132812427
3. `gaussNd` – ∫_[0,1]^d e^(−‖x‖²) dx = ((√π/2)·erf(1))^d (wielowymiarowa)
4. `peak` – ∫₀¹ 1/(1+25x²) dx = atan(5)/5 (duża wariancja względna – zysk z warstwowania)
5. `heavy` – np. `gauss1d` z dodatkowymi 50 iteracjami sztucznej pracy (`sin/cos`) – do oddzielenia kosztu funkcji od kosztu generatora

**Interfejs CLI (wspólny dla wszystkich wersji):**
```
mc_<backend> --func gauss1d --dim 1 --samples 1e8 --seed 42 --batch 65536 \
             [--threads 8] [--target-rel 1e-4 | --target-abs 1e-5] [--max-samples 1e10] \
             [--strata 64] [--conf 0.95] [--mode full|rng-only|func-only] [--csv out.csv]
```

---

## 2. Struktura katalogów

```
.
├── CMakeLists.txt
├── include/mc/
│   ├── philox.h        # A  – generator counter-based, host+device
│   ├── stats.h         # A  – Stats, Welford, merge, CI, kwantyle z
│   ├── functions.h     # A  – funkcje + wartości dokładne
│   ├── stopping.h      # A  – reguła kończenia
│   ├── stratified.h    # B  – podział na warstwy, alokacja, łączenie wariancji
│   ├── cli.h           # B  – parsowanie argumentów
│   └── output.h        # B  – zapis CSV, timery
├── src/
│   ├── seq/main.cpp    # A  – wersja referencyjna
│   ├── omp/main.cpp    # A
│   ├── mpi/main.cpp    # B
│   └── cuda/main.cu    # B (z pomocą A przy redukcji)
├── tests/              # oboje – testy poprawności
├── bench/              # B  – skrypty uruchamiające eksperymenty (bash/SLURM)
├── analysis/           # A  – Python (pandas, matplotlib): wykresy, tabele
├── results/            # surowe CSV
└── report/             # oboje – raport (LaTeX/Markdown)
```

---

## 3. Format wyniku (CSV)

`backend, func, dim, method(plain|strat), samples, seed, batch, workers, estimate, exact, abs_err, rel_err, std_err, ci_lo, ci_hi, ci_hit(0/1), target, reached(0/1), t_total, t_rng, t_func, t_reduce, samples_per_s`

---

## 4. Kamienie milowe

Czas podany orientacyjnie w tygodniach (T1, T2, …) – dopasujcie do terminu oddania.

### M0 – Setup (T1, początek) — **oboje, ~1 dzień**
- [ ] **A:** założenie repozytorium (komendy w czacie), szkielet katalogów, ten plan
- [ ] **B:** `CMakeLists.txt` z opcjami `-DWITH_OMP/-DWITH_MPI/-DWITH_CUDA`, `.gitignore`
- [ ] **Oboje:** ustalenie środowiska: macOS (`brew install libomp open-mpi cmake`) do dev; **CUDA wymaga maszyny z NVIDIA** (lab uczelniany / serwer / Colab) – B sprawdza dostęp już w T1
- **Kryterium:** `cmake --build` kompiluje pusty `mc_seq` u obu osób.

### M1 – Rdzeń + wersja sekwencyjna (T1–T2) — **głównie A**
- [ ] **A:** `philox.h` + test zgodności z wektorami testowymi Random123; konwersja do `double` w [0,1)
- [ ] **A:** `stats.h`: Welford, `merge` (Chan), CI, test: merge dwóch połówek = wynik całości
- [ ] **A:** `functions.h` z 5 funkcjami i wartościami dokładnymi
- [ ] **A:** `src/seq` – pętla po partiach, timery, wynik
- [ ] **B:** `cli.h`, `output.h` (CSV, `std::chrono` / `MPI_Wtime` / `cudaEvent`)
- [ ] **B:** pierwsze testy w `tests/`: błąd maleje ~1/√N, |err| < 4·std_err
- **Kontrakt na koniec M1:** nagłówki `philox.h`, `stats.h`, `functions.h` są **zamrożone** – B buduje na nich MPI/CUDA.

### M2 – Wersje równoległe, stała liczba próbek (T2–T3)
- [ ] **A – OpenMP:** partie rozdzielone `schedule(static)` / `dynamic`, lokalne `Stats` per wątek, scalanie (własna redukcja lub `declare reduction`); pomiar osobno `t_reduce`
- [ ] **B – MPI:** proces `r` dostaje rozłączny zakres partii; `MPI_Reduce` na strukturze `{n, mean, M2}` (własny `MPI_Op` z `merge` lub redukcja `n, Σf, Σf²` + uwaga o stabilności); opcjonalnie MPI+OpenMP hybrydowo
- [ ] **B – CUDA:** kernel: wątek generuje próbki dla swoich indeksów → wartość f → redukcja w bloku (shared memory / warp shuffle) → druga faza redukcji; tryby `--mode`:
  - `rng-only` (sumowanie samych liczb losowych – koszt generatora),
  - `func-only` (punkty deterministyczne, np. siatka/sekwencja `i/N` – koszt funkcji),
  - `full`
- [ ] **A:** to samo rozbicie `rng-only / func-only / full` w wersji CPU (żeby porównać z GPU)
- [ ] **Oboje – test krzyżowy:** dla tego samego `seed` i `N` wszystkie backendy dają **tę samą estymatę** (do błędu zaokrągleń kolejności sumowania) – dzięki Philox to mocny dowód poprawności rozłącznych strumieni
- **Kryterium:** 4 backendy, wspólny CSV, test krzyżowy zielony.

### M3 – Reguła kończenia oparta na dokładności (T3–T4) — **A projektuje, oboje implementują**
- [ ] **A:** `stopping.h`: kryterium `półszerokość CI ≤ ε_rel·|mean|` (lub `≤ ε_abs`), minimalna liczba próbek `n_min`, limit `max_samples`, sprawdzanie co `k` partii (rundy)
- [ ] **A:** integracja w seq + OpenMP (synchronizacja co rundę – koszt synchronizacji vs. „przestrzelenie” liczby próbek)
- [ ] **B:** integracja w MPI (`MPI_Allreduce` statystyk co rundę) i CUDA (kernel na rundę, statystyki na hoście lub flaga na GPU)
- [ ] **A:** analiza wpływu rozmiaru rundy na czas do dokładności; uwaga w raporcie: sekwencyjne zatrzymanie lekko zaniża pokrycie CI – sprawdzić empirycznie
- **Kryterium:** `--target-rel 1e-3` kończy się z `reached=1` na wszystkich backendach.

### M4 – Próbkowanie warstwowe (T4) — **B**
- [ ] **B:** `stratified.h`: K warstw (1D: równe przedziały; Nd: siatka m^d, gdzie m^d = K), alokacja proporcjonalna, opcjonalnie Neymana (pilotaż → n_k ∝ σ_k)
- [ ] **B:** mapowanie indeksu próbki → (warstwa, indeks w warstwie) → Philox – zachowuje rozłączne strumienie
- [ ] **B:** wariancja estymatora `Σ w_k² s_k²/n_k`, CI; integracja z regułą kończenia (we współpracy z A)
- [ ] **A:** wersja OpenMP warstwowania (warstwy rozdzielone między wątki), **B:** MPI i CUDA
- **Kryterium:** przy tym samym budżecie N wariancja warstwowania ≤ wariancja klasyczna dla `sin`, `gauss1d`, `peak`.

### M5 – Eksperymenty (T5) — **B uruchamia, A analizuje**
- [ ] **B:** skrypty `bench/`:
  - N = 10⁴ … 10⁹ (potęgi 10), **100 ziaren** (seed 1…100) – błąd i pokrycie CI
  - skalowanie: OMP 1,2,4,8,16 wątków; MPI 1,2,4,8 procesów; CUDA różne `blockDim`/`gridDim`
  - czas do dokładności: ε_rel ∈ {1e-2, 1e-3, 1e-4, 1e-5}, plain vs strat, każdy backend
  - koszt generatora vs funkcji: `rng-only / func-only / full` dla `gauss1d` i `heavy`
  - wpływ rozmiaru partii `--batch` i rozmiaru rundy
- [ ] **A:** `analysis/plots.py`:
  - |błąd| vs N (log-log, nachylenie −½, plain vs strat) **przy tym samym budżecie losowań**
  - pokrycie CI (odsetek trafień vs nominalne 95%, z przedziałem dla proporcji)
  - czas do dokładności vs ε, próbki/s, przyspieszenie i efektywność
  - wykres słupkowy: udział t_rng / t_func / t_reduce
- **Kryterium:** komplet CSV w `results/`, wszystkie wykresy generowane jednym poleceniem.

### M6 – Raport i prezentacja (T6) — **oboje**
| Sekcja | Osoba |
|---|---|
| Wstęp, metoda MC, CTW, CI, funkcje testowe | A |
| Generator Philox i rozłączność strumieni | A |
| OpenMP, reguła kończenia, analiza rund | A |
| MPI, CUDA, koszt generatora vs funkcji | B |
| Próbkowanie warstwowe | B |
| Wyniki, dyskusja, wnioski (pytanie: strategia próbkowania a czas do dokładności) | oboje |
- [ ] Wzajemna recenzja sekcji, sprawdzenie liczb z CSV, próba prezentacji.

---

## 5. Podsumowanie przypisań

| Osoba A (Konrad) | Osoba B (Kacper) |
|---|---|
| Philox, Stats/CI, funkcje testowe | CMake, CLI, CSV, timery |
| Wersja sekwencyjna (referencja) | MPI (+ opcjonalnie hybryda MPI+OMP) |
| OpenMP (plain + strat) | CUDA (plain + strat, tryby kosztów) |
| Reguła kończenia (projekt + seq/OMP) | Reguła kończenia w MPI/CUDA |
| Rozbicie kosztów rng/func na CPU | Próbkowanie warstwowe (projekt) |
| Analiza i wykresy (Python) | Skrypty benchmarków, uruchamianie na klastrze/GPU |

**Punkty synchronizacji:** koniec M1 (zamrożenie nagłówków), koniec M2 (test krzyżowy), koniec M4 (zamrożenie formatu CSV przed benchmarkami).

## 6. Ryzyka
- **Brak GPU na macOS** → B zabezpiecza dostęp do maszyny z CUDA w T1; awaryjnie OpenCL na Macu (przestarzałe, ale działa) lub Colab.
- **Optymalizacja przez kompilator w trybie `rng-only`** → wynik zawsze sumować i wypisywać.
- **Pomiary czasu** → powtarzać ≥5 razy, raportować medianę; rozgrzewka GPU przed pomiarem.
- **Niedeterminizm redukcji float** → test krzyżowy z tolerancją ~1e-12 względnie, nie równością bitową.

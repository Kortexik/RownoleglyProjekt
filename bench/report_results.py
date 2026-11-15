"""Write a short Polish consultation note from the verified measurement summary."""


def write_report(summary, metadata, output):
    def get(group, **conditions):
        matches = [row for row in summary if row['group'] == group
                   and all(str(row[key]) == str(value) for key, value in conditions.items())]
        if len(matches) != 1:
            raise RuntimeError('Ambiguous report selection: ' + str((group, conditions)))
        return matches[0]

    def number(value, digits=2):
        return f'{value:.{digits}f}'.replace('.', ',')

    def scientific(value):
        return f'{value:.3e}'.replace('.', ',')

    baseline = get('scaling_large_batch', backend='seq', func='gauss1d')['median_seconds']
    eight = get('scaling_large_batch', backend='omp', func='gauss1d', workers=8)['median_seconds']
    ratios = []
    for function in ['sin', 'gauss1d', 'peak']:
        plain = get('fixed_accuracy', backend='seq', func=function, method='plain', fixed_samples='1000000')
        strat = get('fixed_accuracy', backend='seq', func=function, method='strat', fixed_samples='1000000')
        ratios.append(plain['rmse']/strat['rmse'])

    lines = [
        '# Wyniki sekwencyjne i OpenMP', '',
        '## Krótka wypowiedź na konsultacje', '',
        'Mam wersję sekwencyjną i OpenMP ze wspólnym generatorem, statystykami '
        'i regułą zatrzymania. Sprawdziłem zgodność ich wyników dla wielu ziaren. '
        f'Dla gauss1d i 10 milionów próbek osiem wątków dało {number(baseline/eight,1)} '
        'razy krótszy czas niż wersja sekwencyjna. '
        f'Warstwowanie w 1D zmniejszyło RMSE około {number(min(ratios),0)} '
        f'do {number(max(ratios),0)} razy przy tym samym budżecie próbek.', '',
        'Wielkość partii wpływa na koszt synchronizacji i moment sprawdzenia celu. '
        'Duża partia może przyspieszyć długie obliczenie, ale wykonać za dużo próbek '
        'przed zatrzymaniem. Przedział 95% traktuję jako przybliżenie statystyczne, '
        'nie gwarancję błędu.', '',
        '## Wyniki i krótki opis', '',
        f'Pomiary z {metadata["completed_at"][:10]} obejmują {metadata["run_count"]} '
        f'uruchomień obecnego kodu, w tym {metadata["actual_counts"]["omp"]} OpenMP '
        f'i {metadata["actual_counts"]["seq"]} sekwencyjnych. '
        f'Sprzęt udostępnia {metadata["logical_cpus"]} procesorów logicznych. '
        'Każdy przypadek działał w osobnym procesie, w kompilacji Release. '
        'Czasy dotyczą obliczeń i pomijają start procesu oraz zapis CSV. '
        'Nie izolowałem laptopa od innych aplikacji.', '',
        '### Skalowanie liczby wątków', '',
        '`gauss1d`, 10 milionów próbek, partia 1 048 576. Mediany siedmiu pomiarów.', '',
        '| Wersja | Czas | Przyspieszenie względem sekwencyjnej |',
        '|---|---:|---:|',
        f'| Sekwencyjna | {number(baseline*1000)} ms | 1,00× |',
    ]
    for workers in [1, 4, 8, 14]:
        row = get('scaling_large_batch', backend='omp', func='gauss1d', workers=workers)
        label = {1: 'wątek', 4: 'wątki'}.get(workers, 'wątków')
        lines.append(f'| OpenMP, {workers} {label} | {number(row["median_seconds"]*1000)} ms | '
                     f'{number(baseline/row["median_seconds"])}× |')
    fourteen = get('scaling_large_batch', backend='omp', func='gauss1d', workers=14)['median_seconds']
    lines += ['', f'Przejście z 8 do 14 wątków zmieniło czas z {number(eight*1000)} '
              f'do {number(fourteen*1000)} ms. Punktem odniesienia jest teraz osobna '
              'wersja sekwencyjna bez OpenMP.', '',
        '### Dokładność i warstwowanie', '',
        'Milion próbek i 100 ziaren na przypadek. RMSE to pierwiastek ze średniego '
        'kwadratu błędu względem znanej całki. Tabela pokazuje wersję sekwencyjną; '
        'wyniki porównano z OpenMP dla tych samych próbek.', '',
        '| Funkcja | RMSE zwykłego MC | RMSE 64 warstw | Ile razy mniejszy błąd |',
        '|---|---:|---:|---:|',
    ]
    for function in ['sin', 'gauss1d', 'peak']:
        plain = get('fixed_accuracy', backend='seq', func=function, method='plain', fixed_samples='1000000')
        strat = get('fixed_accuracy', backend='seq', func=function, method='strat', fixed_samples='1000000')
        lines.append(f'| {function} | {scientific(plain["rmse"])} | '
                     f'{scientific(strat["rmse"])} | {number(plain["rmse"]/strat["rmse"],1)} |')
    convergence = []
    for function in ['sin', 'gauss1d', 'peak', 'gaussNd']:
        small = get('fixed_accuracy', backend='seq', func=function, method='plain', fixed_samples='10000')
        large = get('fixed_accuracy', backend='seq', func=function, method='plain', fixed_samples='1000000')
        convergence.append(small['rmse']/large['rmse'])
    lines += ['', 'Zwiększenie liczby próbek 100 razy zmniejszyło RMSE zwykłego MC '
              f'około {number(min(convergence),1)} do {number(max(convergence),1)} razy. '
              'Wynik odpowiada zależności 1/√N.', '',
        '### Czas do zadanej dokładności', '',
        '`peak`, cel 0,0001, partia 65 536. OpenMP używa czterech wątków. '
        'Mediany pięciu pomiarów.', '',
        '| Wersja i metoda | Liczba próbek | Czas |', '|---|---:|---:|',
    ]
    for backend in ['seq', 'omp']:
        for method in ['plain', 'strat']:
            row = get('targets', backend=backend, func='peak', method=method, target='0.0001')
            label = ('Sekwencyjna' if backend == 'seq' else 'OpenMP') + ', '
            label += 'zwykłe MC' if method == 'plain' else '64 warstwy'
            count = f'{int(row["median_samples"]):,}'.replace(',', ' ')
            lines.append(f'| {label} | {count} | {number(row["median_seconds"]*1000,3)} ms |')
    lines += ['', 'Cel dotyczy połowy szerokości oszacowanego przedziału. '
              'Czasy poniżej milisekundy są orientacyjne.', '',
        '### Wielkość partii', '',
        '`peak`, 64 warstwy, cel 0,001 i cztery wątki OpenMP. Mediany pięciu pomiarów.', '',
        '| Wielkość partii | Próbki do zatrzymania | Czas |', '|---:|---:|---:|',
    ]
    for batch in [4096, 65536, 1048576]:
        row = get('batch_target', backend='omp', workers=4, batch=batch)
        lines.append(f'| {batch} | {int(row["median_samples"])} | '
                     f'{number(row["median_seconds"]*1000,3)} ms |')
    coverages = [row['coverage'] for row in summary if row['group'] == 'fixed_accuracy'
                 and row['backend'] == 'seq' and row['fixed_samples'] == '1000000']
    lines += ['', 'Przy milionie próbek przedziały zawierały dokładną całkę w '
              f'{number(min(coverages)*100,0)} do {number(max(coverages)*100,0)} procent '
              'uruchomień. Każdy przypadek obejmował 100 ziaren. `reached=1` oznacza '
              'spełnienie kryterium statystycznego, a nie gwarancję rzeczywistego błędu.', '',
        'Osobne koszty generatora, funkcji i agregacji znajdują się w `summary.csv`. '
        'Pomiary generatora i funkcji sumują wyniki do checksum, bez liczenia wariancji. '
        'Ich czasów nie sumujemy jako części czasu pełnego MC.', '',
    ]
    (output/'Wyniki_i_konsultacje.md').write_text('\n'.join(lines), encoding='utf-8')

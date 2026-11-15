"""Build current sources, run fixed experiment cases, validate and save results."""

import argparse
from collections import Counter, defaultdict
from datetime import datetime
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import time
from zoneinfo import ZoneInfo

from report_results import write_report

ROOT = Path(__file__).resolve().parents[1]
ZONE = ZoneInfo('Europe/Warsaw')
EXPECTED_COUNTS = {'omp': 3560, 'seq': 3145}


def source_hashes():
    paths = [ROOT / 'CMakeLists.txt']
    for directory in ('include', 'src', 'tests', 'external', 'bench'):
        paths.extend(p for p in (ROOT / directory).rglob('*')
                     if p.is_file() and '__pycache__' not in p.parts)
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(paths)}


def binary_hashes():
    return {name: hashlib.sha256((ROOT/'build'/name).read_bytes()).hexdigest()
            for name in ('mc_experiments', 'mc_seq', 'mc_omp')}


def cmake_command():
    configured = os.environ.get('MC_CMAKE') or shutil.which('cmake')
    if configured:
        return configured
    cache = ROOT / 'build/CMakeCache.txt'
    if cache.exists():
        for line in cache.read_text().splitlines():
            if line.startswith('CMAKE_COMMAND:INTERNAL='):
                candidate = line.split('=', 1)[1]
                if Path(candidate).is_file():
                    return candidate
    raise RuntimeError('Install CMake or set MC_CMAKE to its executable path.')


def build_and_test(output):
    cmake = cmake_command()
    commands = [
        [cmake, '-S', str(ROOT), '-B', str(ROOT / 'build'), '-DCMAKE_BUILD_TYPE=Release'],
        [cmake, '--build', str(ROOT / 'build'), '--parallel'],
        [cmake, '--build', str(ROOT / 'build'), '--target', 'mc_experiments', '--parallel'],
        [str(Path(cmake).with_name('ctest')), '--test-dir', str(ROOT / 'build'),
         '--output-on-failure'],
    ]
    # If cmake was resolved through PATH, its sibling may need PATH lookup as well.
    if not Path(commands[-1][0]).is_file():
        commands[-1][0] = shutil.which('ctest') or commands[-1][0]
    with (output / 'checks.txt').open('w') as stream:
        for command in commands:
            result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
            stream.write('$ ' + ' '.join(command) + '\n' + result.stdout + result.stderr)
            if result.returncode:
                raise RuntimeError('Build/test failed; see ' + str(output / 'checks.txt'))


def wilson(hits, total):
    z = 1.96
    p = hits / total
    denominator = 1 + z*z / total
    center = (p + z*z/(2*total)) / denominator
    radius = z * math.sqrt(p*(1-p)/total + z*z/(4*total*total)) / denominator
    return center-radius, center+radius


def validate_row(row, case):
    if None in row or any(value is None for value in row.values()):
        raise RuntimeError('Malformed CSV row')
    for key in ('case_id', 'group', 'backend', 'mode'):
        if row[key] != case[key]:
            raise RuntimeError('Experiment manifest mismatch: ' + key)
    if float(row['t_total']) <= 0 or int(row['samples']) < 2:
        raise RuntimeError('Invalid sample count or time')
    expected_workers = '1' if row['backend'] == 'seq' else row['requested_workers']
    if row['workers'] != expected_workers:
        raise RuntimeError('Requested worker count was not available')
    if row['mode'] != 'full':
        if any(row[key] for key in ('estimate', 'exact', 'abs_err', 'rel_err', 'std_err',
                                   'ci_lo', 'ci_hi', 'ci_hit', 'target', 'reached')):
            raise RuntimeError('Benchmark reported Monte Carlo statistics')
        if not math.isfinite(float(row['checksum'])):
            raise RuntimeError('Non-finite checksum')
        return
    estimate, exact, error = (float(row[key]) for key in ('estimate', 'exact', 'abs_err'))
    if not math.isclose(error, abs(estimate-exact), rel_tol=1e-10, abs_tol=1e-15):
        raise RuntimeError('Wrong reference error')
    standard_error = float(row['std_err'])
    if not math.isfinite(standard_error) or standard_error < 0:
        raise RuntimeError('Invalid standard error')
    if not math.isclose(float(row['ci_hi'])-estimate, 1.96*standard_error,
                        rel_tol=1e-8, abs_tol=1e-14):
        raise RuntimeError('Wrong confidence interval')
    if row['target']:
        if int(row['samples']) > int(row['max_samples']):
            raise RuntimeError('Sample limit exceeded')
        reached = int(row['samples']) >= 4096 and 1.96*standard_error <= float(row['target'])
        if reached != (row['reached'] == '1'):
            raise RuntimeError('Wrong stopping decision')
    if row['group'] == 'limits' and (row['samples'] != '10003' or row['reached'] != '0'):
        raise RuntimeError('Limit check failed')


def validate_series(rows):
    reproducibility = defaultdict(list)
    paired = defaultdict(dict)
    for row in rows:
        if row['group'] == 'reproducibility':
            reproducibility[row['func'], row['method']].append(row)
        if row['group'] in ('fixed_accuracy', 'stopped_coverage', 'targets', 'limits'):
            key = tuple(row[k] for k in ('group', 'func', 'method', 'seed', 'batch', 'target', 'repeat'))
            if row['group'] == 'fixed_accuracy':
                key += (row['samples'],)
            paired[key][row['backend']] = row
    for group in reproducibility.values():
        for field in ('estimate', 'std_err'):
            values = [float(row[field]) for row in group]
            if max(values)-min(values) > 1e-12:
                raise RuntimeError('Results depend on partitioning or backend')
    for group in paired.values():
        if set(group) != {'seq', 'omp'}:
            raise RuntimeError('Missing sequential/OpenMP comparison')
        left, right = group['seq'], group['omp']
        if left['samples'] != right['samples'] or left['reached'] != right['reached']:
            raise RuntimeError('Sequential/OpenMP stopping or budget mismatch')
        if abs(float(left['estimate'])-float(right['estimate'])) > 1e-12:
            raise RuntimeError('Sequential/OpenMP estimate mismatch')


def summarize(rows, output):
    keys = ['group', 'backend', 'func', 'dim', 'method', 'mode', 'workers', 'batch', 'target']
    buckets = defaultdict(list)
    for row in rows:
        fixed = row['samples'] if row['group'] not in ('targets', 'stopped_coverage', 'batch_target') else ''
        buckets[tuple(row[key] for key in keys)+(fixed,)].append(row)
    summary = []
    for key, group in sorted(buckets.items()):
        item = dict(zip(keys+['fixed_samples'], key))
        times = [float(row['t_total']) for row in group]
        item.update(runs=len(group), median_seconds=statistics.median(times),
                    min_seconds=min(times), max_seconds=max(times),
                    median_samples=statistics.median(int(row['samples']) for row in group),
                    reached=sum(row['reached'] == '1' for row in group),
                    median_wall_seconds=statistics.median(float(row['wall_seconds']) for row in group))
        if group[0]['mode'] == 'full':
            errors = [float(row['abs_err']) for row in group]
            hits = sum(int(row['ci_hit']) for row in group)
            low, high = wilson(hits, len(group))
            item.update(rmse=math.sqrt(statistics.mean(error*error for error in errors)),
                        mean_abs_error=statistics.mean(errors),
                        mean_std_err=statistics.mean(float(row['std_err']) for row in group),
                        coverage=hits/len(group), coverage_lo=low, coverage_hi=high)
        summary.append(item)
    fields = list(dict.fromkeys(key for row in summary for key in row))
    with (output / 'summary.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summary)
    (output / 'summary.json').write_text(json.dumps(summary, indent=2))
    return summary


def run(output, smoke=False):
    output.mkdir(parents=True, exist_ok=False)
    before = source_hashes()
    build_and_test(output)
    binary = ROOT / 'build/mc_experiments'
    env = dict(os.environ, OMP_DYNAMIC='FALSE', OMP_THREAD_LIMIT='16')
    manifest_text = subprocess.run([str(binary)], input='', text=True,
                                   capture_output=True, check=True, env=env).stdout
    manifest = list(csv.DictReader(io.StringIO(manifest_text)))
    counts = dict(Counter(row['backend'] for row in manifest))
    if counts != EXPECTED_COUNTS:
        raise RuntimeError('Unexpected experiment counts: ' + str(counts))
    if smoke:
        selected = {}
        for row in manifest:
            selected.setdefault((row['group'], row['backend'], row['mode']), row)
        manifest = list(selected.values())
    metadata = dict(status='running', started_at=datetime.now(ZONE).isoformat(),
                    smoke=smoke, source_root=str(ROOT), source_sha256=before,
                    platform=platform.platform(), machine=platform.machine(),
                    logical_cpus=os.cpu_count(), expected_counts=counts,
                    env_overrides={key: env[key] for key in ('OMP_DYNAMIC', 'OMP_THREAD_LIMIT')},
                    inherited_openmp={key: value for key, value in os.environ.items()
                                      if key.startswith(('OMP_', 'KMP_'))},
                    timing='Fresh process per case; randomized fixed case order; Release build. '
                           't_total excludes process startup, preset generation and CSV output. '
                           'wall_seconds includes process startup. No CPU pinning.')
    cache = {}
    for line in (ROOT/'build/CMakeCache.txt').read_text().splitlines():
        if '=' in line and not line.startswith(('//', '#')):
            key, value = line.split('=', 1)
            key = key.split(':', 1)[0]
            if key in ('CMAKE_CXX_COMPILER', 'CMAKE_CXX_FLAGS', 'CMAKE_CXX_FLAGS_RELEASE',
                       'CMAKE_BUILD_TYPE', 'CMAKE_GENERATOR'):
                cache[key] = value
    metadata['build_configuration'] = cache
    compiler = cache.get('CMAKE_CXX_COMPILER')
    if compiler:
        metadata['compiler_version'] = subprocess.run([compiler, '--version'],
            text=True, capture_output=True, check=True).stdout.splitlines()[0]
    metadata['binary_sha256'] = binary_hashes()
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2))
    rows = []
    with (output / 'manifest.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(manifest[0]))
        writer.writeheader()
        writer.writerows(manifest)
    with (output / 'raw.csv').open('w', newline='') as stream:
        writer = None
        for position, case in enumerate(manifest, 1):
            start = time.perf_counter()
            result = subprocess.run([str(binary)], input=case['case_id']+'\n', env=env,
                                    text=True, capture_output=True, check=True, timeout=120)
            elapsed = time.perf_counter()-start
            parsed = list(csv.DictReader(io.StringIO(result.stdout)))
            if len(parsed) != 1:
                raise RuntimeError('Expected one measurement row')
            row = parsed[0]
            validate_row(row, case)
            row['wall_seconds'] = elapsed
            if writer is None:
                writer = csv.DictWriter(stream, fieldnames=list(row))
                writer.writeheader()
            writer.writerow(row)
            rows.append(row)
            if position % 500 == 0 or position == len(manifest):
                stream.flush()
                print(f'Measurements {position}/{len(manifest)}', flush=True)
    if before != source_hashes():
        raise RuntimeError('Sources changed during measurements; do not publish this run')
    if metadata['binary_sha256'] != binary_hashes():
        raise RuntimeError('Binaries changed during measurements; do not publish this run')
    if not smoke:
        validate_series(rows)
    summary = summarize(rows, output)
    metadata.update(completed_at=datetime.now(ZONE).isoformat(), run_count=len(rows),
                    actual_counts=dict(Counter(row['backend'] for row in rows)))
    if not smoke:
        write_report(summary, metadata, output)
    metadata['status'] = 'complete'
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2))
    print('Verified', len(rows), 'measurements in', output, flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'results'/datetime.now(ZONE).strftime('%Y-%m-%d'))
    parser.add_argument('--smoke', action='store_true', help='Short verification run; not final results')
    arguments = parser.parse_args()
    run(arguments.output.resolve(), arguments.smoke)

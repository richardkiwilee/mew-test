import os, subprocess, sys, time, threading
from concurrent.futures import ThreadPoolExecutor, as_completed

BASE = 'D:/mew-test/wiki-asset'
OUT = f'{BASE}/sprites'
os.makedirs(OUT, exist_ok=True)

items = []
with open(f'{BASE}/_tmp/download.tsv', encoding='utf-8') as f:
    for line in f:
        p = line.rstrip('\n').split('\t')
        if len(p) < 7:
            continue
        items.append({'safe': p[0], 'url': p[1], 'size': int(p[2]), 'ext': p[3],
                      'group': p[4], 'page': p[5], 'why': p[6]})

lock = threading.Lock()
results = []
done = [0]
t0 = time.time()

def fetch(it):
    path = os.path.join(OUT, it['safe'])
    if os.path.exists(path) and os.path.getsize(path) > 0:
        with lock:
            done[0] += 1
        return {**it, 'status': 'SKIP-EXISTS', 'bytes': os.path.getsize(path)}
    cmd = ['curl', '-s', '-L', '--retry', '2', '--connect-timeout', '15', '--max-time', '120',
           '-A', 'Mozilla/5.0 research-bot', '-w', '%{http_code}', '-o', path, it['url']]
    try:
        r = subprocess.run(cmd, capture_output=True, timeout=180)
        code = r.stdout.decode('utf-8', 'replace').strip()
    except subprocess.TimeoutExpired:
        code = 'TIMEOUT'
    b = os.path.getsize(path) if os.path.exists(path) else 0
    ok = code == '200' and b > 0
    if not ok and os.path.exists(path):
        try:
            os.remove(path)
        except OSError:
            pass
    with lock:
        done[0] += 1
        n = done[0]
        if n % 250 == 0:
            el = time.time() - t0
            print(f"  {n}/{len(items)}  {el:.0f}s", flush=True)
    return {**it, 'status': f'HTTP{code}' if ok else f'FAIL-{code}', 'bytes': b}

workers = 12
with ThreadPoolExecutor(max_workers=workers) as ex:
    futs = [ex.submit(fetch, it) for it in items]
    for fu in as_completed(futs):
        results.append(fu.result())

with open(f'{BASE}/_tmp/download_results.tsv', 'w', encoding='utf-8') as f:
    for r in results:
        f.write('\t'.join([r['safe'], str(r['bytes']), r['status'], r['ext'], r['group'], r['page'], r['why'], r['url']]) + '\n')

fails = [r for r in results if r['status'].startswith('FAIL') or r['status'].startswith('TIMEOUT')]
print(f"DONE: {len(results)} attempted, {len(fails)} failures, elapsed {time.time()-t0:.0f}s")
for r in fails[:30]:
    print('  FAIL', r['status'], r['safe'])

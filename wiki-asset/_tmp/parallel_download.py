import os, subprocess, time, sys

BASE = 'D:/mew-test/wiki-asset'
OUT = f'{BASE}/sprites'
os.makedirs(OUT, exist_ok=True)

items = []
with open(f'{BASE}/_tmp/download.tsv', encoding='utf-8') as f:
    for line in f:
        p = line.rstrip('\n').split('\t')
        if len(p) >= 7:
            items.append({'safe': p[0], 'url': p[1]})

todo = [it for it in items if not (os.path.exists(os.path.join(OUT, it['safe'])) and os.path.getsize(os.path.join(OUT, it['safe'])) > 0)]
print(f"{len(items)} total, {len(items)-len(todo)} already downloaded, {len(todo)} to fetch", flush=True)

CHUNK = 300
start_chunk = int(sys.argv[1]) if len(sys.argv) > 1 else 0
t0 = time.time()
chunks = [todo[i:i+CHUNK] for i in range(0, len(todo), CHUNK)]
for ci in range(start_chunk, len(chunks)):
    chunk = chunks[ci]
    cfg_path = f'{BASE}/_tmp/pchunk_{ci:04d}.cfg'
    with open(cfg_path, 'w', encoding='utf-8') as f:
        for it in chunk:
            f.write(f'url = "{it["url"]}"\n')
            f.write(f'output = "{OUT}/{it["safe"]}"\n')
    cmd = ['curl', '-s', '--parallel', '--parallel-max', '14', '--retry', '2',
           '--connect-timeout', '15', '--max-time', '120',
           '-A', 'Mozilla/5.0 research-bot', '-K', cfg_path]
    subprocess.run(cmd, timeout=900)
    miss = sum(1 for it in chunk if not os.path.exists(os.path.join(OUT, it['safe'])))
    el = time.time() - t0
    print(f"chunk {ci}/{len(chunks)-1} done  missing={miss}  elapsed={el:.0f}s", flush=True)

print("ALL CHUNKS DONE", flush=True)

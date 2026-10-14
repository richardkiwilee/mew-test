#!/bin/bash
# Fast parallel download using curl config files + connection reuse.
# Usage: parallel_download.sh <chunk_index_start>
export PYTHONIOENCODING=utf-8
BASE="D:/mew-test/wiki-asset"
python - "$@" <<'PYEOF'
import sys
BASE = 'D:/mew-test/wiki-asset'
items = []
with open(f'{BASE}/_tmp/download.tsv', encoding='utf-8') as f:
    for line in f:
        p = line.rstrip('\n').split('\t')
        if len(p) >= 7:
            items.append((p[0], p[1]))
start = int(sys.argv[1]) if len(sys.argv) > 1 else 0
CHUNK = 400
chunks = [items[i:i+CHUNK] for i in range(0, len(items), CHUNK)]
for ci in range(start, len(chunks)):
    cfg = [f'--retry', f'2', f'--connect-timeout', f'15', f'--max-time', f'120',
           f'-A', f'Mozilla/5.0 research-bot']
    with open(f'{BASE}/_tmp/chunk_{ci:03d}.cfg', 'w', encoding='utf-8') as f:
        for safe, url in chunks[ci]:
            f.write(f'url = "{url}"\n')
            f.write(f'output = "{BASE}/sprites/{safe}"\n')
    print(f'wrote chunk {ci} ({len(chunks[ci])} files)')
print(f'TOTAL chunks {len(chunks)}')
PYEOF

import os, json, collections, datetime

BASE = 'D:/mew-test/wiki-asset'
OUT = f'{BASE}/sprites'

def gif_frames(path):
    try:
        d = open(path, 'rb').read()
    except OSError:
        return -1
    if not d.startswith((b'GIF87a', b'GIF89a')):
        return -1
    i = 6
    flags = d[10]
    if flags & 0x80:
        i = 13 + 3 * (2 ** ((flags & 7) + 1))
    else:
        i = 13
    frames = 0
    while i < len(d):
        b = d[i]
        if b == 0x3B:
            break
        if b == 0x2C:
            frames += 1
            i += 1
            if i + 8 > len(d):
                break
            lf = d[i + 8]
            i += 9
            if lf & 0x80:
                i += 3 * (2 ** ((lf & 7) + 1))
            if i >= len(d):
                break
            i += 1  # LZW min code size
            while i < len(d) and d[i] != 0:
                i += d[i] + 1
            i += 1
        elif b == 0x21:
            i += 2
            while i < len(d) and d[i] != 0:
                i += d[i] + 1
            i += 1
        else:
            break
    return frames

def is_animated(path, ext):
    if ext == 'gif':
        return gif_frames(path) > 1
    if ext == 'png':
        d = open(path, 'rb').read()
        return b'acTL' in d
    if ext == 'webp':
        d = open(path, 'rb').read(64)
        return b'ANIM' in open(path, 'rb').read(1024)
    if ext == 'svg':
        d = open(path, 'rb').read(4096)
        return False  # static vector asset
    return False

results = []
with open(f'{BASE}/_tmp/download.tsv', encoding='utf-8') as f:
    for line in f:
        p = line.rstrip('\n').split('\t')
        if len(p) < 7:
            continue
        results.append({'safe': p[0], 'url': p[1], 'bytes': int(p[2]), 'ext': p[3],
                        'group': p[4], 'page': p[5], 'why': p[6], 'status': '-'})

MAGIC = {
    'svg': (b'<svg', b'<?xml', b'<SVG'),
    'png': (b'\x89PNG',),
    'gif': (b'GIF87a', b'GIF89a'),
    'webp': (b'RIFF',),
    'jpg': (b'\xff\xd8',), 'jpeg': (b'\xff\xd8',),
}

def magic_ok(path, ext):
    head = open(path, 'rb').read(16)
    if ext == 'svg':
        head2 = open(path, 'rb').read(2048)
        return any(m in head2 for m in MAGIC['svg'])
    return any(head.startswith(m) for m in MAGIC.get(ext, (b'',)))

rows = []
for r in results:
    path = os.path.join(OUT, r['safe'])
    exists = os.path.exists(path)
    real = os.path.getsize(path) if exists else 0
    anim = False
    note = ''
    if exists and real > 0:
        try:
            if not magic_ok(path, r['ext']):
                note = 'BAD-FORMAT (content not matching extension)'
        except Exception:
            note = 'READ-ERROR'
        try:
            anim = is_animated(path, r['ext']) if note == '' else False
        except Exception:
            anim = False
        if note == '' and real <= 1024:
            note = 'small (<=1KB)'
    else:
        exists = False
    rows.append({**r, 'bytes': real, 'animated': anim, 'exists': exists, 'note': note})

bad = [r for r in rows if r['note'].startswith('BAD-FORMAT') or r['note'] == 'READ-ERROR']
print('BAD FORMAT COUNT:', len(bad))
for r in bad[:20]:
    print('  BAD', r['safe'], r['bytes'])

ok = [r for r in rows if r['exists'] and r['bytes'] > 0]
small = [r for r in ok if r['bytes'] <= 1024]
failed = [r for r in rows if not r['exists'] or r['bytes'] == 0]
anim_gif = [r for r in ok if r['ext'] == 'gif' and r['animated']]
static_gif = [r for r in ok if r['ext'] == 'gif' and not r['animated']]
anim_png = [r for r in ok if r['ext'] == 'png' and r['animated']]
anim_webp = [r for r in ok if r['ext'] == 'webp' and r['animated']]

with open(f'{BASE}/sprite-manifest.csv', 'w', encoding='utf-8') as f:
    f.write('filename,bytes,format,animated,group,source_page,purpose,note,url\n')
    for r in sorted(rows, key=lambda x: (x['group'], x['safe'])):
        f.write(f"{r['safe']},{r['bytes']},{r['ext']},{'yes' if r['animated'] else 'no'},"
                f"{r['group']},{r['page']},{r['why']},{r['note']},{r['url']}\n")

# interactive/level-editor extras discovered via misc pattern
print(f"verified: {len(ok)} ok, {len(failed)} failed/missing, {len(small)} <=1KB")
print(f"animated gif: {len(anim_gif)}, static gif: {len(static_gif)}, animated png(APNG): {len(anim_png)}, animated webp: {len(anim_webp)}")
by_group = collections.defaultdict(lambda: [0, 0, 0])
for r in ok:
    by_group[r['group']][0] += 1
    by_group[r['group']][1] += r['bytes']
    by_group[r['group']][2] += 1 if r['animated'] else 0
for g, (c, s, a) in sorted(by_group.items(), key=lambda x: -x[1][1]):
    print(f"  {g:32s} {c:5d} {s/1e6:8.2f} MB  animated={a}")
for r in failed[:30]:
    print('  FAILED:', r['safe'], r['status'], r['url'])

json.dump([{k: v for k, v in r.items()} for r in rows],
          open(f'{BASE}/_tmp/verified_rows.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=0)

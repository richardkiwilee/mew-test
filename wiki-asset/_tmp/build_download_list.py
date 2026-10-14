import json, re, collections

BASE = 'D:/mew-test/wiki-asset'
rows = []
with open(f'{BASE}/_tmp/allfiles.tsv', encoding='utf-8') as f:
    for line in f:
        parts = line.rstrip('\n').split('\t')
        if len(parts) < 4:
            continue
        rows.append({'name': parts[0], 'size': int(parts[1]), 'mime': parts[2], 'url': parts[3]})

page_map = json.load(open(f'{BASE}/_tmp/page_file_map.json', encoding='utf-8'))  # "File:X" -> [pages]

DEMO = ['Rat', 'Lil_Rat', 'Bat', 'Glass_Spitter', 'Cat_Caller', 'Flea', 'Fly', 'Fly_Swarm',
        'Kitten', 'Leaper', 'Lumpy', 'Maggot', 'Mangy', 'Pinky', 'Pooter', 'Tom_Tom', 'Water_Kitten']
demo_pat = re.compile(r'^ENEMY_(' + '|'.join(re.escape(d) for d in DEMO) + r')([_.]|$)')

GOOD_EXT = ('svg', 'png', 'gif', 'webp')

def ext(n):
    return n.rsplit('.', 1)[-1].lower()

def group_of(n):
    p = re.match(r'^([A-Za-z]+)[._]', n)
    p = (p.group(1) if p else '').upper()
    if n.startswith('Natural_'):
        return 'Natural (cat body parts)'
    if n.startswith('Mutation'):
        return 'Mutation'
    if p in ('ABILITY',): return 'ABILITY'
    if p in ('ITEM',): return 'ITEM'
    if p in ('STATUS',): return 'STATUS'
    if p in ('TILE',): return 'TILE'
    if p in ('UI',): return 'UI'
    if p in ('DISORDER',): return 'DISORDER'
    if p in ('WEATHER',): return 'WEATHER'
    if p in ('MAPICON',): return 'MAPICON'
    if p in ('CATPART',): return 'CATPART'
    if p in ('OBJECT',): return 'OBJECT'
    if p in ('CLASS',): return 'CLASS'
    if p in ('BOSS', 'MINIBOSS'): return 'BOSS/MINIBOSS icons'
    if p in ('ENEMY',): return 'ENEMY'
    return p or 'MISC'

GROUP_DEFAULT_PAGE = {
    'ABILITY': 'Abilities', 'ITEM': 'Items', 'STATUS': 'Status Effects', 'TILE': 'Tiles',
    'UI': 'UI (misc)', 'DISORDER': 'Traps/Disorders (misc)', 'WEATHER': 'Elements (misc)',
    'MAPICON': 'Map icons (misc)', 'CATPART': 'Cats', 'OBJECT': 'Tiles', 'CLASS': 'Classes',
    'BOSS/MINIBOSS icons': 'Enemies', 'Natural (cat body parts)': 'Cats',
    'Mutation': 'Cats (mutations)', 'ENEMY': 'Enemies', 'MISC': 'Misc',
}

def selected(r):
    n = r['name']
    e = ext(n)
    if e not in GOOD_EXT:
        return False, None
    if demo_pat.match(n):
        return True, 'demo-enemy full sprite set'
    if n.startswith('ENEMY_') and 'Icon' in n and e == 'svg':
        return True, 'enemy roster icon'
    if (n.startswith('BOSS_') or n.startswith('MINIBOSS_')) and (e == 'svg'):
        return True, 'boss/miniboss art (svg, incl. icons)'
    for pre in ('ABILITY_', 'ITEM_', 'STATUS_', 'TILE_', 'UI_', 'DISORDER_', 'WEATHER_', 'MAPICON_', 'CATPART_', 'OBJECT_', 'CLASS_', 'Natural_', 'Mutation'):
        if n.startswith(pre):
            return True, f'{pre.rstrip("_")} assets'
    if 'level_editor' in n.lower() or 'Sprite' in n:
        return True, 'misc game sprite'
    return False, None

def safe_name(n):
    out = re.sub(r'[:*?"<>|]', '_', n)
    out = out.strip().rstrip('.')
    return out

dl = []
seen = set()
for r in rows:
    ok, why = selected(r)
    if not ok:
        continue
    fn = safe_name(r['name'])
    if fn.lower() in seen:
        i = 2
        stem, _, e = fn.rpartition('.')
        while f'{stem}_{i}.{e}'.lower() in seen:
            i += 1
        fn = f'{stem}_{i}.{e}'
    seen.add(fn.lower())
    g = group_of(r['name'])
    pages = page_map.get('File:' + r['name'], [])
    pages = [p for p in pages if p not in ('Rats',)]
    src = pages[0] if pages else GROUP_DEFAULT_PAGE.get(g, g)
    dl.append({'name': r['name'], 'safe': fn, 'size': r['size'], 'url': r['url'],
               'ext': ext(r['name']), 'group': g, 'page': src, 'why': why})

with open(f'{BASE}/_tmp/download.tsv', 'w', encoding='utf-8') as f:
    for d in dl:
        f.write('\t'.join([d['safe'], d['url'], str(d['size']), d['ext'], d['group'], d['page'], d['why']]) + '\n')
with open(f'{BASE}/_tmp/download_meta.json', 'w', encoding='utf-8') as f:
    json.dump(dl, f, ensure_ascii=False, indent=0)

by_group = collections.Counter()
size_by_group = collections.Counter()
for d in dl:
    by_group[d['group']] += 1
    size_by_group[d['group']] += d['size']
print(f"TOTAL selected: {len(dl)} files, {sum(d['size'] for d in dl)/1e6:.1f} MB")
for g, c in by_group.most_common():
    print(f"  {g:32s} {c:5d}  {size_by_group[g]/1e6:8.2f} MB")

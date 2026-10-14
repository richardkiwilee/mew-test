import json, os, collections, datetime

BASE = 'D:/mew-test/wiki-asset'
PAGES_DIR = f'{BASE}/wiki-pages'
OUT = f'{BASE}/sprites'

pages = [l.strip() for l in open(f'{BASE}/_tmp/pages_core.txt', encoding='utf-8') if l.strip()]
pages += [l.strip() for l in open(f'{BASE}/_tmp/pages_enemy.txt', encoding='utf-8') if l.strip()]
redirects = json.load(open(f'{BASE}/_tmp/redirects.json', encoding='utf-8'))
rows = json.load(open(f'{BASE}/_tmp/verified_rows.json', encoding='utf-8'))

def safe(page):
    return page.replace(' ', '_').replace(':', '_').replace('*', '_').replace('?', '_').replace('"', '_').replace('<', '_').replace('>', '_').replace('|', '_')

ok = [r for r in rows if r['exists'] and r['bytes'] > 0]
failed = [r for r in rows if not r['exists'] or r['bytes'] == 0]
anim = [r for r in ok if r['animated']]
small = [r for r in ok if r.get('note') == 'small (<=1KB)']

by_group = collections.defaultdict(lambda: [0, 0, 0])
for r in ok:
    g = by_group[r['group']]
    g[0] += 1
    g[1] += r['bytes']
    g[2] += 1 if r['animated'] else 0

total_bytes = sum(r['bytes'] for r in ok)
lines = []
A = lines.append
A('# Mewgenics Wiki Asset Manifest')
A('')
A(f'- Source: https://mewgenics.wiki.gg (MediaWiki 1.43.6)')
A(f'- Downloaded: {datetime.date.today().isoformat()} via MediaWiki API (api.php) + curl')
A(f'- Pages saved: {len(pages)} (wikitext, `wiki-pages/`)')
A(f'- Sprite/image files downloaded: {len(ok)} ({total_bytes/1e6:.1f} MB) in `sprites/`')
A(f'- Animated files (multi-frame GIF / APNG / animated WebP): {len(anim)}')
A(f'- Files <=1KB (noted in listing; legitimately tiny icons/sprites): {len(small)}')
A(f'- Failed downloads: {len(failed)}')
A('- Machine-readable full list with URLs: `sprite-manifest.csv`')
A('- API access: fully open, no blocking or authentication encountered.')
A('')
A('## Part 1 - Wiki pages saved (`wiki-pages/*.wiki`)')
A('')
A('| Page | File | Bytes | Notes |')
A('|---|---|---|---|')
for p in pages:
    fn = safe(p) + '.wiki'
    path = os.path.join(PAGES_DIR, fn)
    b = os.path.getsize(path) if os.path.exists(path) else 0
    note = f'redirect to [[{redirects[p]}]]' if p in redirects else ''
    A(f'| {p} | {fn} | {b} | {note} |')
A('')
A('Notes: saved content is raw wikitext as returned by `api.php?action=parse&prop=wikitext`. 5 pages are redirects and were saved with their target content (see Notes). The demo rat enemy is `Lil\' Rat` on the wiki; the pages `Rat`/`Rats` redirect to `Types`.')
A('')
A('## Part 2 - Sprites / images (`sprites/`)')
A('')
A('### Summary by group')
A('')
A('| Group | Files | MB | Animated |')
A('|---|---|---|---|')
for g, (c, s, a) in sorted(by_group.items(), key=lambda x: -x[1][1]):
    A(f'| {g} | {c} | {s/1e6:.2f} | {a} |')
A('')
A('### Key answers')
A('')
en = [r for r in ok if r['group'] == 'ENEMY']
en_anim = [r for r in en if r['animated']]
demo_anim = [r for r in en_anim if r['why'] == 'demo-enemy full sprite set']
anim_gif = [r for r in ok if r['ext'] == 'gif' and r['animated']]
anim_webp = [r for r in ok if r['ext'] == 'webp' and r['animated']]
anim_png = [r for r in ok if r['ext'] == 'png' and r['animated']]
A(f'- **Animated per-enemy sprites EXIST**: {len(en_anim)} animated `ENEMY_*` files downloaded — all {len(demo_anim)} are the complete animation sets for the demo/Alley enemies (the remaining ENEMY files here are static roster icons / level-editor sprites). Naming convention: `ENEMY_<Enemy Name>_<Animation>.gif` with animations like `Idle`, `Attack`, `Death`, `Low_Health`, `Dodge`, `Stunned`, `Bite`, `Kiss`, `Doom`, plus `_Champion` variants (elite versions), e.g. `ENEMY_Bat_Idle.gif`, `ENEMY_Bat_Low_Health_Champion.gif`. Animated GIF sets exist on the wiki for most of the ~307 enemies (~4.8 GB total); only the demo/Alley sets + all enemy roster icons were downloaded here.')
A(f'- **Total animated files downloaded**: {len(anim_gif)} multi-frame GIF + {len(anim_webp)} animated WebP + {len(anim_png)} APNG. No PNG acTL (APNG) files on the wiki for these groups.')
A(f'- **Status icons**: `STATUS_<Status>_Icon.svg` ({len([r for r in ok if r["group"]=="STATUS"])} files, all downloaded; 2 of them are animated `.webp`: Random Stat Up/Down).')
A(f'- **Tile sprites**: `TILE_<Tile>_Tile_Icon.svg` + `TILE_<Tile>_Icon.*` ({len([r for r in ok if r["group"]=="TILE"])} files, all downloaded).')
A(f'- **Ability icons**: `ABILITY_<Ability>.svg` ({len([r for r in ok if r["group"]=="ABILITY"])} files, all downloaded).')
A('- **Cat sprites**: `CLASS_<Class>_{Idle,Start,Victory,Unlock}.gif/png` (65 files, 42 animated) + `Natural_body.<N>.svg` cat body-part parts (2835) + `CATPART_*` + `Mutation_*`.')
A('- Also fully downloaded: item icons (`ITEM_*`), UI art (`UI_*`), disorder icons (`DISORDER_*`), weather (`WEATHER_*`), animated map icons (`MAPICON_*`), objects/traps (`OBJECT_*`), boss/miniboss svg art, and ~364 per-unit level-editor sprites (`<NAME> level editor.png`).')
A('')

A('### Sprite file listing')
A('')
A('`animated` = multi-frame GIF / APNG / animated WebP (counted by frame/chunk scan of file bytes).')
A('')
A('| File | Bytes | Format | Animated | Source page (from wiki) | Group | Note |')
A('|---|---|---|---|---|---|---|')
for r in sorted(ok, key=lambda x: (x['group'], x['safe'])):
    A(f"| {r['safe']} | {r['bytes']} | {r['ext']} | {'yes' if r['animated'] else 'no'} | {r['page']} | {r['group']} | {r.get('note','')} |")
A('')
if failed:
    A('### Failed downloads')
    A('')
    A('| File | Status | URL |')
    A('|---|---|---|')
    for r in failed:
        A(f"| {r['safe']} | {r['status']} | {r['url']} |")
    A('')
else:
    A('### Failed downloads')
    A('')
    A('None - all selected files downloaded successfully.')
    A('')

A('## Notes / scope')
A('')
A('- The wiki hosts ~26,250 files (~11.9 GB) total, including large animated WebP full-resolution enemy clips, background art, audio (`.ogg` voice/SFX), and video. This asset pack deliberately selects game-art subsets and skips: `Voice_*` (audio), `BG_*` (backgrounds), `NPC_*`, `BOSS_*/MINIBOSS_*` full animation sets (huge; only icons/svg kept), `FURNITURE_*`, `EVENTS_*`, `RADIO_*`, `DIALOGUE_*`, `ACHIEVEMENT_*`, `INTRO_*`, `MAP_*` and non-image media.')
A('- Full per-enemy animated GIF sets (all 307 enemies, ~4.8 GB) can be fetched the same way from `https://mewgenics.wiki.gg/api.php?action=query&list=allimages&aiprefix=ENEMY_<Name>&format=json`.')
A('- File names were sanitized for Windows (characters `: * ? " < > |` replaced with `_`). Underscores match the wiki file names (`ENEMY_Bat_Idle.gif` -> `File:ENEMY Bat Idle.gif` on the wiki).')
A('- `_tmp/` contains the download tooling and reference data: `allfiles.tsv` (complete inventory of all 26,253 files on the wiki with sizes/URLs), `page_files.json` (images used by each saved page), `download_meta.json` (what each downloaded file is and why), `verified_rows.json` (verification results incl. animation flags).')
A('- Demo enemy pages on the wiki: Lil\' Rat (ID `Rat`), Bat, Flea, Fly, Kitten, Leaper, Lumpy, Maggot, Mangy, Pinky, Pooter, Tom Tom, Water Kitten, Cat Caller (Category:The Alley Enemies), plus Glass Spitter. Note: `Rat`/`Rats` wiki pages redirect to the `Types` (Unit Types) page.')

open(f'{BASE}/MANIFEST.md', 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
print('MANIFEST.md written:', len('\n'.join(lines)), 'chars')
print(f'ok={len(ok)} failed={len(failed)} animated={len(anim)}')

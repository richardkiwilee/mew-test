import json, subprocess, urllib.parse

PAGES = [l.strip() for l in open(r'D:/mew-test/wiki-asset/_tmp/pages_core.txt', encoding='utf-8') if l.strip()]
PAGES += [l.strip() for l in open(r'D:/mew-test/wiki-asset/_tmp/pages_enemy.txt', encoding='utf-8') if l.strip()]

mapping = {}
page_files = {}
for page in PAGES:
    files = []
    cont = ''
    for _ in range(10):
        url = (f'https://mewgenics.wiki.gg/api.php?action=query&titles={urllib.parse.quote(page)}'
               f'&prop=images&imlimit=500&format=json&redirects=1{cont}')
        out = subprocess.run(['curl', '-s', '--retry', '2', '--connect-timeout', '15', '--max-time', '60',
                              '-A', 'Mozilla/5.0 research-bot', url], capture_output=True)
        d = json.loads(out.stdout.decode('utf-8'))
        for rid, p in d.get('query', {}).get('pages', {}).items():
            files += [im['title'] for im in p.get('images', [])]
        c = d.get('continue', {})
        if not c:
            break
        cont = '&' + urllib.parse.urlencode(c)
    page_files[page] = files
    for f in files:
        mapping.setdefault(f, set()).add(page)
    print(f"{page:22s} -> {len(files)} files")

json.dump({k: sorted(v) for k, v in mapping.items()},
          open(r'D:/mew-test/wiki-asset/_tmp/page_file_map.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
json.dump(page_files,
          open(r'D:/mew-test/wiki-asset/_tmp/page_files.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print('TOTAL unique files mapped:', len(mapping))

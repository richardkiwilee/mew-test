#!/bin/bash
# Fetch wikitext for a list of pages from mewgenics.wiki.gg
export PYTHONIOENCODING=utf-8
BASE="D:/mew-test/wiki-asset"
OUT="$BASE/wiki-pages"
LIST="$1"
mkdir -p "$OUT"
: > "$BASE/_tmp/page_fetch_log.tsv"
while IFS= read -r page; do
  [ -z "$page" ] && continue
  enc=$(python -c "import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1]))" "$page")
  safe=$(python -c "
import re,sys
n=sys.argv[1]
n=n.replace(' ','_')
n=re.sub(r'[:*?\"<>|]','_',n)
print(n)
" "$page")
  outfile="$OUT/$safe.wiki"
  code=$(curl -s -w "%{http_code}" --retry 2 --connect-timeout 15 --max-time 60 -A "Mozilla/5.0 research-bot" \
    "https://mewgenics.wiki.gg/api.php?action=parse&page=$enc&prop=wikitext&format=json&redirects=1" \
    -o "$BASE/_tmp/pg.json")
  ok=$(python -c "
import json
try:
    d=json.load(open('$BASE/_tmp/pg.json'))
    wt=d.get('parse',{}).get('wikitext',{}).get('*')
    if wt is None:
        print('MISSING')
    else:
        open(r'$outfile','w',encoding='utf-8').write(wt)
        print('OK %d' % len(wt.encode('utf-8')))
except Exception as e:
    print('ERR %s' % e)
")
  echo -e "$page\t$code\t$ok\t$outfile" >> "$BASE/_tmp/page_fetch_log.tsv"
  echo "$page -> $ok"
  sleep 0.3
done < "$LIST"

import json,urllib.request,time,pathlib
from collections import defaultdict
items=json.load(open('hardware/design.json')); codes=sorted({p['lcsc'] for p in items if p['lcsc']})
out={}
for code in codes:
 try:
  req=urllib.request.Request('https://wmsc.lcsc.com/ftps/wm/product/detail?productCode='+code,headers={'User-Agent':'Mozilla/5.0'})
  d=json.load(urllib.request.urlopen(req,timeout=15))
  if d.get('code')!=200: print(code,d.get('code'));continue
  out[code]=d['result']
  r=d['result']; print(code,r['productModel'],r['encapStandard'],r['productDescEn'],flush=True)
 except Exception as e: print(code,e,flush=True)
 time.sleep(.5)
pathlib.Path('research/catalog.json').write_text(json.dumps(out,ensure_ascii=False,indent=2))

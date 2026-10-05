from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import urllib.request,re,json,subprocess
parts={'battery':'C964881','jack':'C95562','uvlo':'C2461421','pmos':'C110499','midi-opto':'C191867','inductor':'C142484','crystal':'C20625731','usb':'C165948','opamp':'C2058050'}
def fetch(kv):
 name,code=kv
 try:
  html=urllib.request.urlopen(urllib.request.Request('https://www.lcsc.com/datasheet/'+code+'.pdf',headers={'User-Agent':'Mozilla/5.0'}),timeout=20).read().decode()
  url=json.loads(re.search(r'<script id="__NEXT_DATA__"[^>]*>(.*?)</script>',html).group(1))['props']['pageProps']['previewPdfUrl']
  data=urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=20).read()
  if not data.startswith(b'%PDF'): return name,'not PDF'
  Path('research/'+name+'.pdf').write_bytes(data)
  subprocess.run(['pdftotext','-layout','research/'+name+'.pdf','research/'+name+'.txt'],check=True)
  return name,url
 except Exception as e: return name,str(e)
print(json.dumps(list(ThreadPoolExecutor(6).map(fetch,parts.items())),indent=2))

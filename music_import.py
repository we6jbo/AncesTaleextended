#!/usr/bin/env python3
"""Local-only Wikimedia Commons audio import; no publication."""
import json, sys, urllib.parse as u, urllib.request as r, pathlib, tempfile, shutil, datetime, re, html
from html.parser import HTMLParser
class Plain(HTMLParser):
 def __init__(self): super().__init__(); self.parts=[]
 def handle_data(self,data): self.parts.append(data)
def plain(s):
 if s is None: return ''
 if not isinstance(s,str):
  return json.dumps(s,ensure_ascii=False) if isinstance(s,(dict,list)) else str(s)
 p=Plain(); p.feed(s); return ' '.join(''.join(p.parts).split())
def request(url):
 return r.urlopen(r.Request(url,headers={'User-Agent':'AncesTaleextended/0.1.2 (local genealogy research; Wikimedia Commons audio import)'}),timeout=45)
def main():
 mode, source, root=sys.argv[1:]; p=u.urlparse(source)
 if p.scheme!='https' or p.netloc!='commons.wikimedia.org' or not p.path.startswith('/wiki/File:'): raise ValueError('Enter an HTTPS Wikimedia Commons File page.')
 title=u.unquote(p.path[len('/wiki/'):]); api='https://commons.wikimedia.org/w/api.php?'+u.urlencode(dict(action='query',titles=title,prop='imageinfo',iiprop='url|mime|size|extmetadata',format='json',formatversion=2))
 with request(api) as f: data=json.load(f)
 page=data['query']['pages'][0]; info=page['imageinfo'][0]; raw=info.get('extmetadata',{}); meta={k:plain(v.get('value','') if isinstance(v,dict) else v) for k,v in raw.items()}; license=meta.get('LicenseShortName',''); allowed=bool(re.fullmatch(r'(?:CC BY(?:-SA)? [1-4]\.\d(?: [A-Za-z]+)?|CC0(?: 1\.0)?|Public domain)',license,re.I)); direct=info['url']; dp=u.urlparse(direct)
 if dp.scheme!='https' or dp.netloc!='upload.wikimedia.org' or not info.get('mime','').startswith(('audio/','application/ogg')) or info.get('size',0)>100*1024*1024: raise ValueError('Unsupported or oversized audio file.')
 record=dict(source_page=source,canonical_page=info.get('descriptionurl'),title=page['title'],direct_audio_url=direct,license=license,license_url=meta.get('LicenseUrl'),artist=meta.get('Artist'),credit=meta.get('Credit'),attribution=meta.get('Attribution'),metadata=meta,raw_metadata=raw,checked_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),review_required=True)
 print(json.dumps(record,ensure_ascii=False,indent=2),flush=True)
 if not allowed: raise ValueError('License not recognized as eligible; no download. Review manually.')
 if mode=='preview': return
 if mode!='download': raise ValueError('Invalid mode')
 dest=pathlib.Path(root)/'Music'; dest.mkdir(exist_ok=True); name=pathlib.Path(u.unquote(dp.path)).name; name=re.sub(r'[^\w. -]','_',name); folder=pathlib.Path(tempfile.mkdtemp(prefix='recording-',dir=dest))
 try:
  with request(direct) as f, open(folder/name,'xb') as out:
   if u.urlparse(f.geturl()).netloc!='upload.wikimedia.org': raise ValueError('Unexpected download host')
   if not f.headers.get_content_type().startswith(('audio/','application/ogg')): raise ValueError('Unexpected content type')
   remaining=100*1024*1024
   while chunk:=f.read(65536):
    remaining-=len(chunk)
    if remaining<0: raise ValueError('Download exceeded limit')
    out.write(chunk)
  if (folder/name).stat().st_size!=info['size']: raise ValueError('File size mismatch')
  (folder/'metadata.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8'); print('Saved locally:',folder)
 except BaseException:
  shutil.rmtree(folder); raise
if __name__=='__main__':
 try: main()
 except Exception as e: print('Import failed:',e,file=sys.stderr); sys.exit(1)

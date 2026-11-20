#!/usr/bin/env python3
"""Private, optional publication review. No upload, license edit, or database access."""
import datetime as dt, fcntl, hashlib, json, os, pathlib, re, subprocess, tempfile, urllib.request, uuid
from zoneinfo import ZoneInfo
P='AncesTaleextended'; UID=str(uuid.uuid5(uuid.NAMESPACE_URL,'urn:j03:AncesTaleextended:publication-review'))
ROOT=pathlib.Path.home()/'.tvv-nov19'; EXE=pathlib.Path.home()/'.local/bin/eventorix'
UTC=dt.timezone.utc; INITIAL=dt.datetime(2026,11,30,18,tzinfo=ZoneInfo('America/Los_Angeles'))
MSG='Consider MIT-0 release of AncesTaleextended: approved GitHub source and AI reuse instructions, Snap Store and approved Flatpak distribution. Human authorization required; no automatic publication.'

def now(): return dt.datetime.now(UTC)
def stamp(): return now().isoformat()
def atomic(path, data):
    fd,tmp=tempfile.mkstemp(dir=path.parent,prefix='.publication-')
    try:
        with os.fdopen(fd,'w') as out:
            out.write(data); out.flush(); os.fsync(out.fileno())
        os.replace(tmp,path)
        fd=os.open(path.parent,os.O_DIRECTORY)
        try: os.fsync(fd)
        finally: os.close(fd)
    finally:
        if os.path.exists(tmp): os.unlink(tmp)
def read(name):
    path=ROOT/name
    if not path.exists(): return {}
    d=json.loads(path.read_text())
    if not isinstance(d,dict) or d.get('project')!=P: raise ValueError(name+' belongs to another project or is invalid; refusing overwrite')
    return d
def save(name,d): atomic(ROOT/name,json.dumps(dict(d,project=P),indent=2)+'\n')
def log(message):
    with (ROOT/'publication-history.log').open('a') as f:
        f.write(stamp()+' '+P+' '+message.replace('\n',' ')+'\n'); f.flush(); os.fsync(f.fileno())
def cli(*args):
    r=subprocess.run([str(EXE),*args],capture_output=True,text=True,timeout=15,check=False)
    if r.returncode: raise RuntimeError('Eventorix '+args[0]+' failed (exit '+str(r.returncode)+'): '+r.stderr[:300])
    return r.stdout

def lookup():
    raw=json.loads(cli('tasks','--source',P,'--include-deleted','--json'))
    if isinstance(raw,list): rows=raw
    elif isinstance(raw,dict) and isinstance(raw.get('tasks'),list): rows=raw['tasks']
    elif isinstance(raw,dict) and isinstance(raw.get('events'),list): rows=raw['events']
    else: raise ValueError('Unrecognized Eventorix task list; refusing mutation')
    if any(not isinstance(r,dict) or 'id' not in r for r in rows): raise ValueError('Invalid task list')
    matches=[r for r in rows if r['id']==UID]
    if len(matches)>1: raise ValueError('Duplicate UUID reported')
    if not matches and any('publication' in str(r.get('message','')).lower() or ('mit-0' in str(r.get('message','')).lower()) for r in rows): raise ValueError('Possible existing publication task with another ID; review manually')
    return matches[0] if matches else None

def parse(s):
    # Eventorix JSON may return Unix epoch timestamps as numbers (seconds or ms).
    # Never silently accept ambiguous units or naive dates.
    if isinstance(s, bool):
        raise ValueError('Invalid boolean due timestamp')
    if isinstance(s, (int, float)) or (isinstance(s, str) and s.strip().isdigit()):
        n = float(s)
        if not (946684800 <= n <= 4102444800):
            if 946684800000 <= n <= 4102444800000:
                n /= 1000
            else:
                raise ValueError('Unsupported numeric Eventorix due timestamp')
        return dt.datetime.fromtimestamp(n, UTC)
    if not isinstance(s, str):
        raise ValueError('Unsupported Eventorix due timestamp type')
    d = dt.datetime.fromisoformat(s.replace('Z', '+00:00'))
    if d.tzinfo is None:
        raise ValueError('Due timestamp has no offset')
    return d

def due_ok(row,target): return row and abs((parse(row['due_at'])-target).total_seconds())<=5

def schedule(target,create):
    # Absolute timestamp first; relative seconds fallback only after a fresh lookup.
    for fmt in ('absolute','seconds'):
        row=lookup()
        if due_ok(row,target): return row
        if row and row.get('status') not in ('pending','due','claimed'): raise ValueError('Task not active; no mutation')
        delay=max(1,round((target-now()).total_seconds()))
        after=target.astimezone(UTC).isoformat().replace('+00:00','Z') if fmt=='absolute' else str(delay)+'s'
        args=['reschedule','--id',UID,'--after',after] if row else ['schedule','--id',UID,'--source',P,'--assigned-to','all','--message',MSG,'--after',after]
        if not row and not create: raise ValueError('Existing task disappeared; no recreation')
        try: cli(*args)
        except Exception as e: log(str(e))
        row=lookup()
        if due_ok(row,target): return row
    raise ValueError('Exact due time was not verified; retained UUID and requested target for next launch')

def fetch(url,headers=None):
    req=urllib.request.Request(url,headers={'User-Agent':P+'-publication-review/0.1.3b',**(headers or {})})
    with urllib.request.urlopen(req,timeout=8) as r:
        if not r.url.startswith('https://'): raise ValueError('Non-HTTPS response')
        data=r.read(4*1024*1024+1)
        if len(data)>4*1024*1024: raise ValueError('Response too large')
        return data

def github(c):
    repo=c['repo']; commit=c['commit']; files=c['approved_sha256']
    if not re.fullmatch(r'[\w.-]+/[\w.-]+',repo) or not re.fullmatch('[0-9a-f]{40}',commit): raise ValueError('Expected approved repository and full commit SHA')
    if not isinstance(files,dict) or len(files)<3 or not all(c.get(k) in files for k in ('license_path','ai_instructions_path')): raise ValueError('Approved source, license and AI instructions hashes needed')
    if c['license_path']==c['ai_instructions_path']: raise ValueError('Distinct license and AI files required')
    base='https://api.github.com/repos/'+repo
    if json.loads(fetch(base)).get('private') is not False: raise ValueError('Repository not confirmed public')
    if json.loads(fetch(base+'/commits/'+commit)).get('sha')!=commit: raise ValueError('Commit unavailable')
    from urllib.parse import quote
    for path,digest in files.items():
        if path.startswith('/') or '..' in path.split('/') or not re.fullmatch('[0-9a-f]{64}',digest): raise ValueError('Invalid approved path/hash')
        data=fetch('https://raw.githubusercontent.com/'+repo+'/'+commit+'/'+quote(path,safe='/'))
        if hashlib.sha256(data).hexdigest()!=digest: raise ValueError('Approved file hash differs: '+path)
        if path==c['license_path'] and not (b'MIT-0' in data or b'MIT No Attribution' in data or (b'Permission is hereby granted' in data and b'without restriction' in data and b'THE SOFTWARE IS PROVIDED' in data and b'included in all copies' not in data)): raise ValueError('MIT-0 license text not recognized')
    return {'url':'https://github.com/'+repo+'/tree/'+commit,'commit':commit,'verified_files':len(files)}

def snap(c):
    name=c['name']; version=c['version']; publisher=c['publisher']
    if not re.fullmatch('[a-z0-9-]+',name) or not version or not publisher: raise ValueError('Approved Snap identity required')
    d=json.loads(fetch('https://api.snapcraft.io/v2/snaps/info/'+name,{'Snap-Device-Series':'16'}))
    pub=d.get('publisher') or d.get('snap',{}).get('publisher',{})
    if pub.get('username')!=publisher: raise ValueError('Snap publisher mismatch')
    matches=[x for x in d.get('channel-map',[]) if x.get('channel',{}).get('risk')=='stable' and x.get('version')==version]
    if not matches: raise ValueError('Expected stable Snap version not found')
    return {'url':'https://snapcraft.io/'+name,'version':version,'publisher':publisher}

def flatpak(c):
    app=c['app_id']; version=c['version']
    if c.get('channel')!='flathub': raise ValueError('Alternative channel requires a separately implemented verification adapter')
    if not re.fullmatch(r'[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+){2,}',app) or not version: raise ValueError('Approved Flatpak identity required')
    d=json.loads(fetch('https://flathub.org/api/v2/appstream/'+app))
    if d.get('id')!=app: raise ValueError('Flatpak identity not confirmed')
    if not any(isinstance(x,dict) and x.get('version')==version for x in d.get('releases',[])): raise ValueError('Expected public Flatpak release not confirmed')
    return {'url':'https://flathub.org/apps/'+app,'version':version}

def verify(name,fn,config,authorized):
    file=name+'-status.json'; s=read(file); s.update(checked_at=stamp(),current_result='unverified')
    try:
        if not authorized: raise ValueError('Owner-approved release verification configuration not supplied')
        evidence=fn(config); s.update(current_result='verified',last_verified_at=stamp(),last_verified=evidence,verified_config=config); s.pop('reason',None)
    except Exception as e: s['reason']=str(e)
    save(file,s); log(name+' '+s['current_result']+(' '+s.get('reason','') if s['current_result']!='verified' else ''))
    return s['current_result']=='verified'

def run():
    os.umask(0o077); ROOT.mkdir(mode=0o700,parents=True,exist_ok=True)
    with (ROOT/'.ancestaleextended.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        # Preflight all existing status documents before changing any.
        for n in ('publication','github','snap','flatpak','eventorix'): read(n+'-status.json')
        p=read('publication-status.json'); e=read('eventorix-status.json')
        p.setdefault('approval',{}); a=p['approval']; configs=a.get('targets',{})
        authorized=a.get('owner_approved') is True and a.get('license')=='MIT-0'
        results={n:verify(n,fn,configs.get(n,{}),authorized) for n,fn in [('github',github),('snap',snap),('flatpak',flatpak)]}
        complete=all(results.values()); p.update(task_uuid=UID,initial_due=INITIAL.isoformat(),current_results=results,publication_complete=complete,checked_at=stamp())
        save('publication-status.json',p)
        e.update(task_uuid=UID,checked_at=stamp(),current_result='unverified')
        try:
            if not EXE.is_file() or not os.access(EXE,os.X_OK): raise ValueError('Private Eventorix executable unavailable')
            row=lookup()
            if complete:
                if row and row.get('status') not in ('completed','complete','done'):
                    if row.get('status') not in ('pending','due','claimed'): raise ValueError('Unexpected task state; no completion')
                    cli('complete','--id',UID); row=lookup()
                if not row or row.get('status') not in ('completed','complete','done'): raise ValueError('Task completion not verified')
            else:
                if not row and e.get('ever_observed'): raise ValueError('Previously observed task missing; no duplicate created')
                target=None
                if e.get('requested_due'): target=parse(e['requested_due'])
                elif not row: target=INITIAL if INITIAL>now() else now()+dt.timedelta(days=14)
                elif row.get('status') in ('pending','due','claimed') and parse(row['due_at'])<=now(): target=now()+dt.timedelta(days=14)
                elif row.get('status') not in ('pending','due','claimed'): raise ValueError('Task manually closed/deleted; review required')
                if target:
                    if target<=now(): target=now()+dt.timedelta(days=14)
                    e['requested_due']=target.isoformat(); save('eventorix-status.json',e)
                    row=schedule(target,not e.get('ever_observed')); e.pop('requested_due',None)
            e.update(current_result='verified',last_verified_at=stamp(),last_verified_task=row,ever_observed=True); e.pop('reason',None)
        except Exception as ex: e['reason']=str(ex)
        save('eventorix-status.json',e); log('Eventorix '+e['current_result']+' '+e.get('reason',''))
        due=(e.get('last_verified_task') or {}).get('due_at','not verified')
        atomic(ROOT/'AI-STATUS.md',f'# {P} publication continuity\n\nRead all five JSON files and publication-history.log before development.\n\nTask UUID: {UID}\nLast verified due: {due}\nCurrent Eventorix result: {e["current_result"]}\nCurrent verification: {results}\nPublication complete: {complete}\n\nPending: obtain owner approval and verified target details; inspect unverified reasons. Never publish, edit license, commit, or upload automatically. Private data and configuration must be excluded from any separately sanitized public build.\n')
if __name__=='__main__':
    try: run()
    except Exception as e:
        try: log('Review skipped: '+str(e))
        except Exception: pass

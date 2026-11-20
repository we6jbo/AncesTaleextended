#!/usr/bin/env python3
"""Private AncesTaleextended 0.1.7 tree bridge. No automatic remote writes.

Original family database, incoming xwtree and additive ghtree remain independent.
"""
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import urllib.request
from email.utils import parsedate_to_datetime

HOME = Path.home()
APP = HOME / 'Projects' / 'AncesTaleextended'
STATE = HOME / '.local' / 'state' / 'ancestaleextended-v017'
HISTORY = HOME / '.local' / 'share' / 'AncesTaleextended' / 'private-history'
OLD_HISTORY = HOME / '.local' / 'state' / 'ancestaleextended-v015' / 'versions'
PRIVATE = HOME / '.local' / 'share' / 'ancestaleextended-v0.1.4'
USB_SETTINGS = Path('/opt/usbsettings/settings.txt')
REMOTE_TREE = 'https://raw.githubusercontent.com/joneal97/Pedigree/refs/heads/master/xwtree.json'
REMOTE_DATE = 'https://raw.githubusercontent.com/we6jbo/AncesTaleextended/refs/heads/master/date.txt'
CATEGORIES = ('aliases', 'identifiers', 'imports', 'people', 'provisional_facts', 'relationships', 'record_overrides', 'duplicate_records', 'spouse_relationships', 'duplicate_reviews', 'captured_people')
LOG = STATE / 'operations.log'
# authorizej03 is an external hardware-aware executable; never inspect its internals.
AUTHORIZE_COMMAND = 'authorizej03'

def now(): return dt.datetime.now().astimezone()
def stamp(): return now().strftime('%Y%m%d-%H%M%S-%f')
def note(message):
    STATE.mkdir(parents=True, exist_ok=True)
    with LOG.open('a', encoding='utf-8') as f: f.write(now().isoformat() + ' ' + message + '\n')

def safe_json(path):
    if path.is_symlink() or path.stat().st_size > 30_000_000: raise ValueError('Unsafe or oversized tree: '+str(path))
    data = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(data, dict): raise ValueError('Tree root must be a JSON object')
    for cat in CATEGORIES:
        if cat in data and not isinstance(data[cat], list): raise ValueError('Invalid list category: '+cat)
    return data

def atomic(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink(): raise ValueError('Refusing symlink: '+str(path))
    fd, temp = tempfile.mkstemp(prefix='.tree-', dir=path.parent)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd,'wb') as out:
            out.write(content);out.flush();os.fsync(out.fileno())
        os.replace(temp,path)
    finally:
        if os.path.exists(temp): os.unlink(temp)

def dump(data): return (json.dumps(data, ensure_ascii=False, indent=2)+'\n').encode('utf-8')
def hashfile(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def unique_merge(*lists):
    result=[];seen=set()
    for items in lists:
        for item in items:
            signature=json.dumps(item,sort_keys=True,ensure_ascii=False)
            if signature not in seen: seen.add(signature);result.append(item)
    return result

def local_families():
    people={}; identifiers=[]; captures=[]; relationship=[]
    # Parent research JSON records are authoritative only for the fields they contain.
    p=PRIVATE/'missing_family_records.json'
    if p.exists():
        tree=json.loads(p.read_text(encoding='utf-8'))
        for person in tree.get('people',[]):
            if not isinstance(person,dict): continue
            name=str(person.get('name') or '').strip()
            if not name:continue
            people.setdefault(name,{'person_no':0,'display_name':name,'details':'','name_permanent':0})
            tgs=[x for x in person.get('identifiers',[]) if isinstance(x,str) and re.fullmatch(r'TG\d{6}',x)]
            for tg in tgs:identifiers.append({'display_name':name,'code':tg,'kind':'TG','permanent':0})
            captures.append({'display_name':name,'tg_number':tgs[0] if tgs else '',
                             'born':str(person.get('birth_date') or person.get('birth_year') or ''),
                             'died':str(person.get('death_date') or person.get('death_year') or ''),
                             'parents':[x for x in (person.get('father_name'),person.get('mother_name')) if x],
                             'spouse':[],'children':[],'details':'','source':'AncesTaleextended private missing-family JSON'})
    # Family groups contain one row per family, with person and relation field sets.
    db=PRIVATE/'family_groups.db'
    if db.exists():
        with sqlite3.connect(f'file:{db}?mode=ro',uri=True) as conn:
            tables={r[0] for r in conn.execute("SELECT name FROM sqlite_master WHERE type='table'")}
            if 'family_groups' in tables:
                conn.row_factory=sqlite3.Row
                for row in conn.execute('SELECT * FROM family_groups'):
                    d=dict(row); roles=['person','father','mother','spouse','child']; names={}
                    for role in roles:
                        name=' '.join(str(d.get(role+'_'+piece) or '').strip() for piece in ('first_name','last_name')).strip()
                        if not name:continue
                        names[role]=name
                        details='; '.join(f'{label}: {d.get(role+"_"+label)}' for label in ('birth_date','birth_location','death_date','death_location') if d.get(role+'_'+label))
                        people.setdefault(name,{'person_no':0,'display_name':name,'details':details,'name_permanent':0})
                        captures.append({'display_name':name,'tg_number':'','born':str(d.get(role+'_birth_date') or d.get(role+'_birth_year') or ''),'died':str(d.get(role+'_death_date') or d.get(role+'_death_year') or ''),'parents':[],'spouse':[],'children':[],'details':details,'source':'AncesTaleextended private family_groups.db'})
                    for parent in ('father','mother'):
                        if 'person' in names and parent in names:
                            relationship.append({'child':names['person'],'parent':names[parent],'relation':'parent','confidence':0.5,'permanent':0})
                        if 'child' in names and 'person' in names:
                            relationship.append({'child':names['child'],'parent':names['person'],'relation':'parent','confidence':0.5,'permanent':0})
    return {'people':list(people.values()),'identifiers':unique_merge(identifiers),'captured_people':unique_merge(captures),'relationships':unique_merge(relationship)}

def git(*args, check=True):
    # All commands target the local-only history root, not the public source repo.
    p=subprocess.run(['git','-C',str(HISTORY),*args],capture_output=True,text=True)
    if check and p.returncode:
        raise RuntimeError('Private version control failed: '+p.stderr.strip())
    return p

def ensure_local_history():
    # Never accept symlinks or a remote. A local-only Git repository is mandatory.
    for path in (HISTORY.parent,HISTORY,HISTORY/'.git'):
        if path.is_symlink():
            raise RuntimeError('Unsafe private Git history symlink: '+str(path))
    HISTORY.mkdir(mode=0o700,parents=True,exist_ok=True)
    os.chmod(HISTORY,0o700)
    if (HISTORY/'.git').exists() and not (HISTORY/'.git').is_dir():
        raise RuntimeError('Invalid private Git metadata directory')
    if not (HISTORY/'.git').exists():
        git('init','-q')
    if git('remote',check=True).stdout.strip():
        raise RuntimeError('Private history has a remote configured; refusing commit to prevent publication')
    # Only intended snapshot paths are staged, never an entire directory.


def require_authorization():
    """Fail closed. Authorization is decided by the installed hardware-aware binary.

    Accept exactly one documented success token; never accept arbitrary substring
    matches such as 'unauthorized', diagnostics, or shell-execution errors.
    No passwords, tokens, or authorization results are persisted.
    """
    executable = shutil.which(AUTHORIZE_COMMAND)
    if not executable:
        raise PermissionError('authorizej03 not found on PATH. No protected operation performed.')
    try:
        result = subprocess.run([executable], capture_output=True, text=True,
                                timeout=15, stdin=subprocess.DEVNULL, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise PermissionError('authorizej03 could not complete safely. Authorization denied.') from exc
    # The user described the success words 'accepted' and 'authorized'.
    # Reject everything else, including 'unauthorized'.
    output = result.stdout.strip()
    if result.returncode != 0 or result.stderr.strip() or output not in ('accepted', 'authorized'):
        raise PermissionError('authorizej03 did not authorize this operation. No protected operation performed.')

def commit_files(label, sourcepaths):
    # Git holds private snapshots only under hidden state, never under public j03-software.
    ensure_local_history()
    repo=HISTORY
    allow={'date.txt','ghtree.json','imported-tree.json','usb-export.json'}
    if len({p.name for p in sourcepaths}) != len(sourcepaths) or any(p.name not in allow for p in sourcepaths):
        raise RuntimeError('Refusing unapproved or duplicate private snapshot filename')
    for src in sourcepaths:
        if src.is_symlink() or not src.is_file():
            raise RuntimeError('Refusing missing or symlinked snapshot source: '+str(src))
        dest=repo/src.name
        atomic(dest,src.read_bytes())
    manifest={'recorded_at':now().isoformat(),'action':label,'files':{p.name:hashfile(p) for p in sourcepaths}}
    atomic(repo/'version-manifest.json',dump(manifest))
    names=[p.name for p in sourcepaths]+['version-manifest.json']
    git('add','--',*names)
    if git('diff','--cached','--quiet',check=False).returncode:
        git('-c','user.name=AncesTaleextended private versioner','-c','user.email=local@localhost','commit','-qm',f'Ver {now().strftime("%Y-%m-%d %H:%M:%S")} {label}')
    return git('rev-parse','--short','HEAD').stdout.strip()

def startup():
    APP.mkdir(parents=True,exist_ok=True)
    datefile=APP/'date.txt'; treefile=APP/'ghtree.json'
    # Never allow incoming xwtree to modify exported ghtree files.
    current=local_families()
    previous=safe_json(treefile) if treefile.exists() else {}
    result={cat:unique_merge(previous.get(cat,[]),current.get(cat,[])) for cat in CATEGORIES}
    # Previous captured entries precede new local copies: nothing is discarded.
    atomic(treefile,dump(result)); atomic(datefile,(now().date().isoformat()+'\n').encode())
    note('startup export written; private commit deferred until authorizej03 authorization')
    print('STARTUP OK',now().isoformat(),'people',len(result['people']),'COMMIT PENDING AUTHORIZATION')
    print('date',datefile,'tree',treefile)

def commit_startup():
    # Require authorization before invoking this action in main().
    datefile=APP/'date.txt'; treefile=APP/'ghtree.json'
    if not datefile.is_file() or not treefile.is_file():
        raise ValueError('Run startup export before committing')
    safe_json(treefile)
    commit=commit_files('startup export',[datefile,treefile])
    note('startup commit completed '+commit)
    print('PRIVATE STARTUP COMMIT OK:',commit)

def configured_usb_path():
    """Read the mountpoint on every operation. Never use a compiled-in USB location.

    Expected settings.txt: USB_MOUNT_PATH=/absolute/mount/path (or USB_PATH=...).
    A single bare absolute mount path is also accepted for simple settings files.
    """
    if not USB_SETTINGS.is_file() or USB_SETTINGS.is_symlink():
        raise ValueError('USB settings file missing or unsafe: '+str(USB_SETTINGS))
    if USB_SETTINGS.stat().st_size > 65536:
        raise ValueError('USB settings file exceeds 64 KB')
    raw = USB_SETTINGS.read_text(encoding='utf-8-sig')
    allowed = {'usb_mount_path','usb_path','mount_path','mountpoint',
               'usb_mountpoint','usb_mount','usb_directory','usb_dir',
               'usb_root','usb_drive_path','usb_location','winpe_path'}
    named = []
    bare = []
    for line in raw.splitlines():
        line=line.strip()
        if not line or line.startswith(('#',';','[')):
            continue
        # Do not eval or execute settings. Reject interpolation and shell constructs.
        pair = re.match(r'^([a-zA-Z][a-zA-Z0-9_-]*)\s*[:=]\s*(.*?)\s*$',line)
        if pair:
            key=pair.group(1).lower().replace('-','_')
            value=pair.group(2).strip().strip('\"\'')
            if key in allowed:
                named.append(value)
        else:
            value=line.strip('\"\'')
            if value.startswith('/'):
                bare.append(value)
    choices = named or bare
    if len(choices)!=1:
        raise ValueError('Expected exactly one USB mount path in '+str(USB_SETTINGS)+
                         '. Use USB_MOUNT_PATH=/absolute/path')
    value=choices[0]
    if not value.startswith('/') or value in ('/','/run','/run/media','/media','/mnt') or any(x in value for x in ('\x00','$','`','\n','\r')):
        raise ValueError('Unsafe or non-absolute USB mount path in settings')
    mount=Path(value)
    if '..' in mount.parts:
        raise ValueError('USB mount path must not contain ..')
    # Mounting a symlink target is unsafe, including symlink parent directories.
    for parent in (mount,*mount.parents):
        if parent.is_symlink():
            raise ValueError('USB mount path contains a symlink: '+str(parent))
    return mount

def usb_path():
    mount=configured_usb_path()
    if not mount.is_mount() or not mount.is_dir():
        raise ValueError('USB is not mounted at configured path '+str(mount)+
                         '. Check '+str(USB_SETTINGS))
    return mount


def mount_usb():
    mount=configured_usb_path()
    if mount.is_mount():
        print('USB ALREADY MOUNTED',mount);return
    result=subprocess.run(['lsblk','-J','-o','PATH,LABEL,TYPE'],text=True,capture_output=True,timeout=12)
    if result.returncode:raise ValueError('Cannot inspect block devices: '+result.stderr.strip())
    devices=json.loads(result.stdout).get('blockdevices',[])
    # Label is taken from the configured mount path, not built into the program.
    label=mount.name
    matching=[]
    def walk(items):
        for device in items:
            if device.get('label')==label and device.get('type')=='part':
                matching.append(device.get('path'))
            walk(device.get('children') or [])
    walk(devices)
    if len(matching)!=1 or not matching[0] or not re.fullmatch(r'/dev/[A-Za-z0-9._/-]+',matching[0]):
        raise ValueError('USB is not mounted and no unique partition with label '+repr(label)+
                         ' was found. Mount it with the file manager at '+str(mount))
    try:
        result=subprocess.run(['udisksctl','mount','-b',matching[0]],text=True,capture_output=True,timeout=30)
    except FileNotFoundError:
        raise ValueError('udisksctl unavailable; mount USB using the file manager')
    if result.returncode or not mount.is_mount():
        raise ValueError('USB mount failed or mounted at another location. '+
                         'Update '+str(USB_SETTINGS)+' to actual mountpoint. '+
                         result.stderr.strip()+' '+result.stdout.strip())
    print('USB MOUNTED',mount)

def prepare_import():
    # Only triggered by explicit user button click; no automatic USB access at startup.
    try:
        mount_usb()
        export_usb()
    except Exception as exc:
        print('USB WARNING:',exc)
    preview()

def candidates():
    found=[]
    sources=[('Downloads',HOME/'Downloads/xwtree.json')]
    try:
        sources.append(('configured USB',configured_usb_path()/'xwtree.json'))
    except Exception as exc:
        print('USB SETTINGS WARNING:',exc)
    for label,p in sources:
        if p.exists() and not p.is_symlink():
            safe_json(p)
            found.append({'source':label,'path':str(p),'modified':dt.datetime.fromtimestamp(p.stat().st_mtime,dt.timezone.utc).isoformat(),'sha256':hashfile(p),'mtime':p.stat().st_mtime})
    # HTTP Last-Modified may be omitted; remote can't be treated as latest without it.
    try:
        req=urllib.request.Request(REMOTE_TREE,headers={'User-Agent':'AncesTaleextended/0.1.6','Accept':'application/json'})
        with urllib.request.urlopen(req,timeout=12) as response:
            if response.geturl()!=REMOTE_TREE:raise ValueError('Unexpected remote redirect')
            content=response.read(20_000_001)
            if len(content)>20_000_000:raise ValueError('Remote tree too large')
            obj=json.loads(content)
            if not isinstance(obj,dict) or any(k in obj and not isinstance(obj[k],list) for k in CATEGORIES):raise ValueError('Invalid remote tree')
            cache=STATE/'remote-xwtree.json'; atomic(cache,content)
            h=response.headers.get('Last-Modified')
            mtime=parsedate_to_datetime(h).timestamp() if h else None
            found.append({'source':'GitHub raw','path':str(cache),'modified':h or 'unknown','sha256':hashfile(cache),'mtime':mtime})
    except Exception as exc:note('remote preview unavailable '+str(exc));print('REMOTE WARNING:',exc)
    return found

def preview():
    found=candidates()
    if not found:raise ValueError('No valid local, USB, or remote xwtree.json candidate')
    ordered=sorted(found,key=lambda a:(a['mtime'] is not None,a['mtime'] or 0),reverse=True)
    # never silently apply based on timestamps; display candidate and hashes
    for i,item in enumerate(ordered):print(f'[{i}] {item["source"]} modified={item["modified"]} sha256={item["sha256"]} path={item["path"]}')
    atomic(STATE/'import-candidates.json',dump({'created_at':now().isoformat(),'candidates':ordered}))
    print('PREVIEW ONLY. Select a candidate in Import tab before applying.')

def apply(index):
    metadata=STATE/'import-candidates.json'
    if not metadata.exists():raise ValueError('Run Preview first')
    record=json.loads(metadata.read_text()); choices=record['candidates']
    if index<0 or index>=len(choices):raise ValueError('Invalid index')
    selected=choices[index];p=Path(selected['path']);tree=safe_json(p)
    if hashfile(p)!=selected['sha256']:raise ValueError('Candidate changed since preview')
    # ghtree PRIMARY, xwtree SECONDARY. Never touch APP/ghtree.json or USB/ghtree.txt.
    original=safe_json(APP/'ghtree.json')
    combined={cat:unique_merge(original.get(cat,[]),tree.get(cat,[])) for cat in CATEGORIES}
    output=STATE/'imported-tree.json'
    atomic(output,dump(combined))
    commit=commit_files('tree import overlay',[APP/'date.txt',APP/'ghtree.json',output])
    note('import source='+selected['source']+' sha='+selected['sha256']+' commit='+commit)
    print('IMPORTED OVERLAY',selected['source'],'person_count',len(combined['people']),'commit',commit)
    print('Overlay path:',output,'(source data unchanged; local records take precedence)')

def export_usb():
    mount=usb_path(); target=mount/'ghtree.txt'
    source=APP/'ghtree.json';safe_json(source)
    if target.is_symlink():raise ValueError('Refusing USB symlink')
    if target.exists():
        # Preserve all existing USB entries. A previously written file is never overwritten.
        prior=safe_json(target)
        latest=safe_json(source)
        result={cat:unique_merge(prior.get(cat,[]),latest.get(cat,[])) for cat in CATEGORIES}
        if dump(result)!=target.read_bytes():
            # Backup before additive merge, and atomically replace after successful verification.
            archive=STATE/'usb-archives'/f'ghtree-{stamp()}.txt'
            atomic(archive,target.read_bytes())
            atomic(target,dump(result))
    else:
        atomic(target,source.read_bytes())
    # Commit recorded USB hash to a local private Git repository, never public remote.
    manifest=STATE/'usb-export.json';atomic(manifest,dump({'exported_at':now().isoformat(),'usb_sha256':hashfile(target),'path':str(target)}))
    commit=commit_files('usb export',[APP/'date.txt',APP/'ghtree.json',manifest]);note('usb export commit='+commit)
    print('USB EXPORT OK',target,'sha256',hashfile(target),'private_commit',commit)

def versions():
    print('LOCAL PRIVATE HISTORY:',HISTORY)
    if not (HISTORY/'.git').is_dir():
        print('No commits yet at this location.')
        if (OLD_HISTORY/'.git').is_dir():
            print('Older v0.1.6 history is still preserved at:',OLD_HISTORY)
        return
    if git('remote').stdout.strip():
        print('WARNING: Unexpected Git remote detected. Commits will be blocked.')
        return
    result=git('log','-12','--date=iso-local','--pretty=format:%h | %ad | %s',check=False)
    if result.returncode:
        print('No commits yet.');return
    print(result.stdout)
    print('Recent private snapshot files:', ', '.join(p.name for p in HISTORY.iterdir() if p.is_file()))


def publish_public_date():
    """Explicit, authorizej03-approved PUBLIC date.txt ONLY. Never stage family files."""
    source = APP / 'date.txt'
    if source.is_symlink() or not source.is_file():
        raise ValueError('Missing/unsafe public date.txt')
    value = source.read_text(encoding='utf-8').strip()
    if not re.fullmatch(r'\d{4}-\d{2}-\d{2}',value):
        raise ValueError('date.txt must contain only YYYY-MM-DD')
    if value != now().date().isoformat():
        raise ValueError('date.txt does not match today; run application startup export again')
    if (APP/'.git').is_symlink() or not (APP/'.git').exists():
        raise ValueError('Main project is not a valid Git repository')
    def public_git(*args, check=True, timeout=30):
        p=subprocess.run(['git','-C',str(APP),*args],capture_output=True,text=True,timeout=timeout)
        if check and p.returncode:
            raise RuntimeError('Main Git operation failed: '+p.stderr.strip()[:500])
        return p
    remote=public_git('remote','get-url','origin').stdout.strip()
    allowed={'https://github.com/we6jbo/AncesTaleextended.git','git@github.com:we6jbo/AncesTaleextended.git'}
    if remote not in allowed:
        raise ValueError('Unexpected public remote, refusing publication: '+remote)
    if public_git('branch','--show-current').stdout.strip()!='master':
        raise ValueError('Expected main source branch master; refusing push')
    staged=public_git('diff','--cached','--name-only').stdout.strip()
    if staged:
        raise ValueError('Existing staged changes detected; unstage/review them before publishing date.txt. No changes made.')
    # Refuse existing local divergence, to prevent sending unrelated/private commits.
    local=public_git('rev-parse','HEAD').stdout.strip()
    upstream=public_git('ls-remote','--heads','origin','master',timeout=25).stdout.strip()
    if not upstream or upstream.split()[0]!=local:
        raise ValueError('Local main branch and GitHub master differ. Refusing to push unrelated commits.')
    # Guard against secret/private material already tracked in this public repository.
    tracked=public_git('ls-files').stdout.splitlines()
    prohibited={'ghtree.json','ghtree.txt','family_groups.db','missing_family_records.json',
                'imported-tree.json','usb-export.json','secret.txt','secrets.txt'}
    if any(Path(f).name in prohibited for f in tracked):
        raise ValueError('Public Git repository tracks private data; refusing any push')
    # Only this file may be included in the new commit. --only excludes unrelated worktree modifications.
    public_git('add','--','date.txt')
    diff=public_git('diff','--cached','--quiet','--','date.txt',check=False)
    if diff.returncode==0:
        print('PUBLIC DATE ALREADY COMMITTED: date.txt='+value)
        return
    if diff.returncode!=1:
        raise RuntimeError('Unable to check staged date.txt safely')
    public_git('-c','user.name=AncesTaleextended date publisher',
               '-c','user.email=local@localhost','commit','--only','-m','Update public date.txt '+value,'--','date.txt')
    commit=public_git('rev-parse','HEAD').stdout.strip()
    changed=public_git('diff-tree','--no-commit-id','--name-only','-r',commit).stdout.splitlines()
    if changed!=['date.txt']:
        raise RuntimeError('SAFETY STOP: committed unexpected files: '+repr(changed))
    try:
        push=public_git('push','origin','master',timeout=65)
    except Exception:
        note('public date commit '+commit+' created, push failed; manual recovery needed')
        raise
    print('PUBLIC DATE PUSH SUCCESS:',commit)
    print('Only date.txt was included in the commit.')
    print('GitHub push output:',(push.stdout+' '+push.stderr).strip()[:500])
    note('public date pushed commit='+commit)

def publish_reviewed_tree():
    """Explicit, authorizej03-approved public ghtree.json publication only.

    Tree data is potentially private; UI confirmation is mandatory before invoking this.
    Never publish any other family files or implicit local commits.
    """
    tree = APP / 'ghtree.json'
    if tree.is_symlink() or not tree.is_file():
        raise ValueError('No regular ghtree.json file to publish')
    data = safe_json(tree)
    if not isinstance(data.get('people'), list):
        raise ValueError('ghtree.json lacks the expected people list')
    if tree.stat().st_size > 10_000_000:
        raise ValueError('Tree is unexpectedly large. Review manually first.')
    if (APP/'.git').is_symlink() or not (APP/'.git').is_dir():
        raise ValueError('Main Git repository missing or unsafe')
    def pg(*args, check=True, timeout=30):
        proc = subprocess.run(['git','-C',str(APP),*args],capture_output=True,text=True,timeout=timeout)
        if check and proc.returncode:
            raise RuntimeError('Git operation failed: '+proc.stderr.strip()[:500])
        return proc
    remote = pg('remote','get-url','origin').stdout.strip()
    if remote not in ('https://github.com/we6jbo/AncesTaleextended.git',
                      'git@github.com:we6jbo/AncesTaleextended.git'):
        raise ValueError('Unexpected remote; refusing to publish tree')
    if pg('branch','--show-current').stdout.strip() != 'master':
        raise ValueError('Main project must be on master')
    if pg('diff','--cached','--name-only').stdout.strip():
        raise ValueError('Unrelated staged changes exist. No files added.')
    local = pg('rev-parse','HEAD').stdout.strip()
    remote_head = pg('ls-remote','--heads','origin','master',timeout=25).stdout.strip()
    if not remote_head or remote_head.split()[0] != local:
        raise ValueError('Local and remote branches differ; refusing unrelated push')
    forbidden = {'family_groups.db','missing_family_records.json','ghtree.txt',
                 'imported-tree.json','usb-export.json','secret.txt','secrets.txt'}
    tracked = pg('ls-files').stdout.splitlines()
    if any(Path(f).name in forbidden for f in tracked):
        raise ValueError('Public repository tracks restricted family/secret files; refusing push')
    # A single path commit cannot include other pending source or personal files.
    pg('add','--','ghtree.json')
    staged = pg('diff','--cached','--name-only').stdout.splitlines()
    if staged != ['ghtree.json']:
        raise RuntimeError('Unexpected staged paths; aborting without commit: '+repr(staged))
    delta = pg('diff','--cached','--quiet','--','ghtree.json',check=False)
    if delta.returncode == 0:
        print('PUBLIC TREE ALREADY MATCHES LOCAL COMMIT: no new tree changes')
        return
    if delta.returncode != 1:
        raise RuntimeError('Could not determine staged tree changes')
    pg('-c','user.name=AncesTaleextended reviewed tree publisher',
       '-c','user.email=local@localhost','commit','--only',
       '-m','Publish explicitly reviewed ghtree.json', '--','ghtree.json')
    commit = pg('rev-parse','HEAD').stdout.strip()
    changed = pg('diff-tree','--no-commit-id','--name-only','-r',commit).stdout.splitlines()
    if changed != ['ghtree.json']:
        raise RuntimeError('SAFETY STOP: unexpected committed files '+repr(changed))
    try:
        result=pg('push','origin','master',timeout=65)
    except Exception:
        note('reviewed ghtree commit '+commit+' created, push failed')
        raise
    print('PUBLIC GHTREE PUSH SUCCESS:',commit)
    print('ONLY ghtree.json was included in this commit.')
    print('GitHub push:',(result.stdout+' '+result.stderr).strip()[:500])
    note('reviewed public tree pushed commit='+commit)

def shutdown():
    datefile=APP/'date.txt'
    if not datefile.exists():print('WARNING: local date.txt missing');return
    local=datefile.read_text().strip()
    try:
        with urllib.request.urlopen(urllib.request.Request(REMOTE_DATE,headers={'User-Agent':'AncesTaleextended/0.1.6'}),timeout=8) as res:
            remote=res.read(1024).decode('utf-8').strip()
        if remote==local:print('REMOTE DATE VERIFIED:',remote)
        else:print('WARNING: GitHub date.txt differs. Local:',local,'Remote:',remote)
    except Exception as exc:print('WARNING: Unable to verify GitHub date.txt:',exc)
    note('shutdown remote date checked local='+local)

def usb_config():
    print('USB configuration file:',USB_SETTINGS)
    print('Configured mount:',configured_usb_path())

def main():
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['startup','commit-startup','preview','prepare-import','mount-usb','apply','export-usb','versions','shutdown','usb-config','publish-date','publish-tree']);parser.add_argument('--index',type=int,default=-1);arg=parser.parse_args()
    try:
        if arg.action in {'commit-startup','prepare-import','apply','export-usb','publish-date','publish-tree'}:
            require_authorization()
        {'startup':startup,'commit-startup':commit_startup,'preview':preview,'prepare-import':prepare_import,'mount-usb':mount_usb,'apply':lambda:apply(arg.index),'export-usb':export_usb,'versions':versions,'shutdown':shutdown,'usb-config':usb_config,'publish-date':publish_public_date,'publish-tree':publish_reviewed_tree}[arg.action]()
    except Exception as exc:
        note(arg.action+' ERROR '+str(exc));print('ERROR:',exc,file=sys.stderr);sys.exit(1)
if __name__=='__main__':main()

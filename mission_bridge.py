#!/usr/bin/env python3
"""Read-only genealogy quest evaluator and SSH website-lead importer.
All new mutable state lives in ~/.local/share/AncesTaleextended/mission/.
"""
import argparse, collections, datetime, hashlib, json, os, pathlib, re, sqlite3, subprocess, sys

ROOT = pathlib.Path(os.environ.get('ANCESTALEEXTENDED_ROOT', str(pathlib.Path.home()/'Projects/AncesTaleextended')))
STATE = pathlib.Path.home()/'.local/share/AncesTaleextended/mission'
GOALS = [
 ('Mary Johannsen','Hinrich Knuttel'),('William L Schorf','Fredericka Schlorff'),
 ('Olive G Stafford','Sarah Greene'),('William Shapley','Hans Bar'),
 ('Mary Rice','Margaret Ellis'),('Amelia Kiehlbauch','Blandina Stoeffler'),
 ('Robert R Hedtke','Friedrich Wilhelm Luedtke'),('Gail Agnes Hillman','David C Platt'),
 ('William R Maynard','William Cline'),('Nancy Jane Lambert','Tabitha Brown'),
 ('Thomas S Bowman','Jacob Craven'),('Rose Ann Prickett','Jesse Shoemaker'),
 ('Albert Franklin Smith','Elizabeth Goodspeed'),('Elizabeth McQuistion','Mary Thorpe'),
 ('James N Elliott','Margaret Scott'),('Sarah McAvoy','Mary Evett'),
 ('Edward McCabe','John McCabe')]

def norm(s):
    return re.sub(r'[^a-z0-9]+',' ',str(s).casefold()).strip()

def load_json(p):
    with open(p,encoding='utf-8') as f:return json.load(f)

def build_graph():
    """Only ID-backed explicit links may complete a verified quest.
    Archived family_group name-only links are research candidates, not verified identity.
    """
    g=load_json(ROOT/'ghtree.json')
    names=collections.defaultdict(set)
    labels={}
    adj=collections.defaultdict(dict)
    for row in g.get('identifiers',[]):
        if not isinstance(row,dict):continue
        code=row.get('code'); name=row.get('display_name')
        if code and name:
            labels[code]=name;names[norm(name)].add(code)
    for row in g.get('people',[]):
        if isinstance(row,dict):
            name=row.get('display_name'); pid=row.get('tg_number') or row.get('code')
            if pid and name:labels[pid]=name;names[norm(name)].add(pid)
    for row in g.get('captured_people',[]):
        if not isinstance(row,dict):continue
        name=row.get('display_name'); pid=row.get('tg_number')
        if pid and name:labels[pid]=name;names[norm(name)].add(pid)
    def resolve(name):
        found=names.get(norm(name),set())
        return next(iter(found)) if len(found)==1 else None
    verified=0; uncertain=0
    for row in g.get('relationships',[]):
        if not isinstance(row,dict):continue
        a=row.get('child_id') or row.get('child_code') or resolve(row.get('child',''))
        b=row.get('parent_id') or row.get('parent_code') or resolve(row.get('parent',''))
        if not a or not b or a==b:uncertain+=1;continue
        if row.get('verified') is not True and (not row.get('permanent') or not row.get('source')):
            uncertain+=1;continue
        adj[a][b]='parent/child';adj[b][a]='parent/child';verified+=1
    for row in g.get('spouse_relationships',[]):
        if not isinstance(row,dict):continue
        a=row.get('person1_id') or row.get('person_a_id') or resolve(row.get('person1',''))
        b=row.get('person2_id') or row.get('person_b_id') or resolve(row.get('person2',''))
        if not a or not b or a==b or not row.get('verified') or not row.get('source'):
            uncertain+=1;continue
        adj[a][b]='spouse';adj[b][a]='spouse';verified+=1
    return names,labels,adj,verified,uncertain

def path_between(start,finish,adj):
    seen={start:None}; queue=collections.deque([start])
    while queue:
        node=queue.popleft()
        if node==finish:
            out=[]
            while node is not None:out.append(node);node=seen[node]
            return list(reversed(out))
        for next_ in adj.get(node,{}):
            if next_ not in seen:seen[next_]=node;queue.append(next_)
    return None

def quests():
    names,labels,adj,verified,uncertain=build_graph()
    results=[]
    for idx,(left,right) in enumerate(GOALS,1):
        a=names.get(norm(left),set());b=names.get(norm(right),set())
        result={'id':idx,'left':left,'right':right,'completed':False,'status':'missing documented path','path':[]}
        if len(a)!=1 or len(b)!=1:
            result['status']='unresolved or ambiguous TG identity'
        else:
            start,finish=next(iter(a)),next(iter(b))
            route=path_between(start,finish,adj)
            if route:
                result['completed']=True;result['status']='verified path'
                result['path']=[{'id':x,'name':labels.get(x,x), 'via':adj[route[i-1]][x] if i else 'start'} for i,x in enumerate(route)]
        results.append(result)
    count=sum(r['completed'] for r in results)
    return {'total':len(GOALS),'completed':count,'remaining':len(GOALS)-count,
            'percentage':round(100*count/len(GOALS),2),'verified_edges':verified,
            'excluded_uncertain_edges':uncertain,'goals':results,'source':str(ROOT/'ghtree.json')}

def db():
    STATE.mkdir(parents=True,exist_ok=True);os.chmod(STATE,0o700)
    c=sqlite3.connect(STATE/'mission.sqlite')
    c.execute('CREATE TABLE IF NOT EXISTS leads (id TEXT PRIMARY KEY, submitted_at TEXT, payload TEXT NOT NULL, status TEXT NOT NULL DEFAULT "unverified", reviewed_at TEXT)')
    c.execute('CREATE TABLE IF NOT EXISTS achievements (id TEXT PRIMARY KEY, announced_at TEXT NOT NULL)')
    c.commit();return c

def read_leads():
    # audit.php starts with a deliberate PHP denial line; content is read only over SSH.
    cmd=['ssh','-o','BatchMode=yes','-o','ConnectTimeout=8','jeremiahai@100.73.131.89',
         'cat /home/jeremiahai/www/family/audit.php']
    p=subprocess.run(cmd,capture_output=True,timeout=18)
    if p.returncode:raise RuntimeError('SSH retrieval failed: '+p.stderr.decode(errors='replace')[:400])
    raw=p.stdout.splitlines(); count=0; skipped=0
    if not raw:raise ValueError('Remote audit empty or wrong format')
    for line in raw[1:]:
        if not line.strip():continue
        try:record=json.loads(line)
        except json.JSONDecodeError:skipped+=1;continue
        if not isinstance(record,dict) or not record.get('id'):skipped+=1;continue
        if record.get('line_type')!='indirect_unspecified':skipped+=1;continue
        value=json.dumps(record,ensure_ascii=False,sort_keys=True)
        with db() as c:
            c.execute('INSERT OR IGNORE INTO leads(id,submitted_at,payload,status) VALUES(?,?,?,?)',
                      (str(record['id']),str(record.get('submitted_at','')),value,'unverified'))
        count+=1
    return {'fetched':count,'skipped':skipped,'warning':'All website leads remain unverified. No authoritative tree modified.'}

def list_leads():
    with db() as c:
        rows=c.execute('SELECT id,submitted_at,payload,status FROM leads ORDER BY submitted_at DESC LIMIT 500').fetchall()
    return [{'id':id,'submitted_at':at,'record':json.loads(p),'status':status} for id,at,p,status in rows]

def set_status(id,status):
    if status not in ('unverified','reviewed','accepted','rejected','more-evidence'):
        raise ValueError('Invalid review status')
    with db() as c:
        updated=c.execute('UPDATE leads SET status=?, reviewed_at=? WHERE id=?',
                (status,datetime.datetime.now(datetime.timezone.utc).isoformat(),id)).rowcount
    return {'updated':updated,'id':id,'status':status,'note':'Status is local review only; no tree updates'}

def outputs(obj):print(json.dumps(obj,ensure_ascii=False))

def main():
    ap=argparse.ArgumentParser();ap.add_argument('action',choices=('quests','leads','fetch-leads','review','announce'));ap.add_argument('arg',nargs='?');ap.add_argument('status',nargs='?');a=ap.parse_args()
    try:
        if a.action=='quests':outputs(quests())
        elif a.action=='leads':outputs({'leads':list_leads()})
        elif a.action=='fetch-leads':outputs(read_leads())
        elif a.action=='review':outputs(set_status(a.arg,a.status))
        elif a.action=='announce':
            # Persist at-most-once announcements; only discoveries confirmed by current graph.
            result=quests(); newly=[]
            with db() as c:
                for g in result['goals']:
                    if not g['completed']:continue
                    fingerprint=hashlib.sha256(json.dumps(g['path'],sort_keys=True).encode()).hexdigest()[:20]
                    key=f"{g['id']}:{fingerprint}"
                    cur=c.execute('INSERT OR IGNORE INTO achievements VALUES (?,?)',(key,datetime.datetime.now(datetime.timezone.utc).isoformat()))
                    if cur.rowcount:newly.append(g)
            outputs({'new':newly,'summary':{'completed':result['completed'],'total':result['total'],'percentage':result['percentage']}})
    except Exception as e:
        print(json.dumps({'error':str(e)}),file=sys.stdout);sys.exit(1)
if __name__=='__main__':main()

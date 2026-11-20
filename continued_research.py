#!/usr/bin/env python3
import argparse, json, os, pathlib, tempfile, shutil, datetime, uuid
ROOT=pathlib.Path(__file__).resolve().parent
FIELDS=('birth_date','birth_location','death_date','death_location','mother_name','father_name')
def load(path):
    d=json.loads(path.read_text()); assert isinstance(d['people'],list)
    for p in d['people']:
        if not isinstance(p,dict) or not isinstance(p.get('name'),str) or not p['name'].strip(): raise ValueError('Invalid person')
    return d
def key(p):
    return (p.get('research_id') or str(p.get('database_record_id') or '')+'|'+str(p.get('geography_serial') or '')+'|'+','.join(sorted(p.get('identifiers') or []))+'|'+p['name'])
def missing(p): return [f for f in FIELDS if not p.get(f)]
def pending(p): return p.get('research_status')!='resolved' or bool(missing(p)) and not p.get('resolution_notes')
def save(path,d):
    backup=path.parent/'.research-backups'; backup.mkdir(mode=0o700,exist_ok=True)
    if path.exists():
        b=backup/(path.name+'.'+uuid.uuid4().hex); shutil.copyfile(path,b); b.chmod(0o600)
    d.setdefault('counts',{})['output_people']=len(d['people'])
    fd,tmp=tempfile.mkstemp(dir=path.parent,prefix='.research-')
    try:
        with os.fdopen(fd,'w') as f: json.dump(d,f,ensure_ascii=False,indent=2); f.flush(); os.fsync(f.fileno())
        load(pathlib.Path(tmp)); os.replace(tmp,path)
    finally:
        if os.path.exists(tmp): os.unlink(tmp)
def prompt(path,persons):
    return f'''Research {json.dumps([{'name':p['name'],'identifiers':p.get('identifiers',[]),'database_record_id':p.get('database_record_id')} for p in persons],ensure_ascii=False)}. Read {path} and {ROOT/'CONTINUED-RESEARCH.md'} first. Follow the evidence, backup, privacy and atomic-update rules. Add additional identified people discovered during research to the same JSON. Never invent identities or overwrite verified facts. Keep unresolved cases checked. Do not publish or modify unrelated files.'''
def gui(path):
    import tkinter as tk
    from tkinter import ttk,messagebox,simpledialog
    root=tk.Tk(); root.title('AncesTaleextended • Continued Research test'); root.geometry('1050x650')
    tabs=ttk.Notebook(root); tabs.pack(fill='both',expand=True); tab=ttk.Frame(tabs,padding=12); tabs.add(tab,text='Continued Research')
    state={'data':None,'stamp':None,'rows':{}}; search=tk.StringVar(); filt=tk.StringVar(value='All'); status=tk.StringVar(); detail=tk.StringVar();
    toolbar=ttk.Frame(tab); toolbar.pack(fill='x'); ttk.Label(toolbar,text='Search').pack(side='left'); ttk.Entry(toolbar,textvariable=search,width=28).pack(side='left',padx=6); ttk.Combobox(toolbar,textvariable=filt,values=['All','Still missing','Resolved'],state='readonly',width=15).pack(side='left')
    ttk.Label(tab,textvariable=status).pack(anchor='w',pady=8)
    tree=ttk.Treeview(tab,columns=('check','name','fields','status'),show='headings',selectmode='extended');
    for c,t,w in [('check','Missing',65),('name','Person',250),('fields','Missing fields',440),('status','Research status',150)]: tree.heading(c,text=t); tree.column(c,width=w)
    tree.pack(fill='both',expand=True); ttk.Label(tab,textvariable=detail,wraplength=980).pack(anchor='w',pady=8)
    def render(*_):
        if state['data'] is None:return
        selected={key(state['rows'][i]) for i in tree.selection() if i in state['rows']}; tree.delete(*tree.get_children()); state['rows']={}; people=state['data']['people']; n=sum(pending(p) for p in people); status.set(f'{len(people)} people  |  {n} still requiring research  |  {len(people)-n} resolved  |  {len(state["data"].get("possible_missing_generations",[]))} separate hypotheses')
        for i,p in enumerate(people):
            if search.get().casefold() not in p['name'].casefold() or filt.get()=='Still missing' and not pending(p) or filt.get()=='Resolved' and pending(p):continue
            iid=str(i); state['rows'][iid]=p; tree.insert('', 'end',iid=iid,values=('☑' if pending(p) else '☐',p['name'],', '.join(missing(p)) or 'None recorded', 'Needs research' if pending(p) else 'Resolved'))
            if key(p) in selected:tree.selection_add(iid)
    def refresh(force=False):
        try:
            stamp=(path.stat().st_mtime_ns,path.stat().st_size,path.stat().st_ino)
            if force or stamp!=state['stamp']:
                d=load(path); state.update(data=d,stamp=stamp); render()
        except Exception as e:status.set(f'Cannot reload JSON; retaining last valid view: {e}')
    def watch():refresh(); root.after(1000,watch)
    def selected():return [state['rows'][i] for i in tree.selection() if i in state['rows']]
    def copy():
        ps=selected() or [p for p in (state['data'] or {}).get('people',[]) if pending(p)]; root.clipboard_clear();root.clipboard_append(prompt(path,ps));detail.set(f'Copied prompt for {len(ps)} people.')
    def add():
        name=simpledialog.askstring('Add person','Full name:',parent=root)
        if not name or not name.strip():return
        try:
            d=load(path)
            if any(p['name'].casefold()==name.strip().casefold() for p in d['people']):raise ValueError('Matching name already exists; review identity first.')
            d['people'].append(dict(name=name.strip(),research_id=str(uuid.uuid4()),identifiers=[],**{f:None for f in FIELDS},spouse_names=[],children=[],source={},research_status='pending',missing_fields=list(FIELDS),research_notes=['Manually added; unverified']))
            save(path,d);refresh(True)
        except Exception as e:messagebox.showerror('Add failed',str(e))
    def resolve():
        ps=selected()
        if len(ps)!=1:return messagebox.showinfo('Research','Select exactly one record.')
        note=simpledialog.askstring('Research decision','Evidence/source and reason to resolve (including any remaining unknown fields):',parent=root)
        if not note or not note.strip():return
        try:
            d=load(path); matches=[p for p in d['people'] if key(p)==key(ps[0])];
            if len(matches)!=1:raise ValueError('Identity ambiguous; not changed')
            p=matches[0];p.update(research_status='resolved',resolution_notes=note.strip(),last_researched=datetime.date.today().isoformat());save(path,d);refresh(True)
        except Exception as e:messagebox.showerror('Update failed',str(e))
    def show(_):
        ps=selected();detail.set(' | '.join(f"{p['name']}: {', '.join(p.get('identifiers',[]))}; last researched: {p.get('last_researched','not recorded')}" for p in ps))
    tree.bind('<<TreeviewSelect>>',show);search.trace_add('write',render);filt.trace_add('write',render)
    for title,fn in [('Refresh JSON',lambda:refresh(True)),('Add person',add),('Copy Vibe prompt',copy),('Mark resolved with evidence',resolve)]:ttk.Button(toolbar,text=title,command=fn).pack(side='left',padx=3)
    watch();root.mainloop()
if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('--data',type=pathlib.Path,default=ROOT/'missing_family_records.json');a.add_argument('--check',action='store_true');args=a.parse_args()
    if args.check:
        d=load(args.data);print(f"{len(d['people'])} people; {sum(pending(p) for p in d['people'])} pending; {len(d.get('possible_missing_generations',[]))} hypotheses")
    else:gui(args.data)

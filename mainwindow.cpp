#include "nov27_speech.h"
#include <QGuiApplication>
#include <QClipboard>
#include "mainwindow.h"
#include "mission_tabs.h"
#include "classical_ai.h"
#include "terminal_control.h"
#include "ui_mainwindow.h"
#include "geographywidget.h"
#include "teachai_events.h"
#include <unistd.h>
#include <QtWidgets>
#include <QtSql>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QUuid>
#include <QProcess>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QTimer>
#include <QQueue>
#include <QSet>
#include <QCloseEvent>
namespace {
const QString code="AncesTaleextended";
const QString root=qEnvironmentVariable("ANCESTALEEXTENDED_ROOT",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
const QStringList roles={"person","father","mother","spouse","child"};
const QStringList fields={"first_name","last_name","birth_year","birth_date","birth_location","death_year","death_date","death_location","source_url"};
QString str(QJsonValue v){return v.isString()?v.toString():QString();}
QString uid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
void log(const QString &s){ QDir().mkpath(root+"/.local-logs"); QFile f(root+"/.local-logs/session.log"); if(f.open(QIODevice::Append|QIODevice::Text)){ f.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner); f.write((QDateTime::currentDateTime().toString(Qt::ISODate)+" "+code+" "+s+"\n").toUtf8()); }}
QByteArray read(const QString &p){QFile f(p); if(!f.open(QIODevice::ReadOnly)){teachAiEvent("file.read",p,"failed");return {};} auto out=f.readAll();teachAiEvent("file.read",p,"ok");return out;}
bool write(const QString &p,const QByteArray &b){QSaveFile f(p); bool ok=f.open(QIODevice::WriteOnly)&&f.write(b)==b.size()&&f.commit();teachAiEvent("file.write",p,ok?"ok":"failed",true);return ok;}
QString key(QJsonObject p){return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(p).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
QStringList missing(QJsonObject p){QStringList m; for(auto f:{"birth_date","birth_location","death_date","death_location","mother_name","father_name"}) if(str(p.value(f)).isEmpty())m<<f;return m;}
bool pending(QJsonObject p){return p.value("research_status").toString()!="resolved"||(!missing(p).isEmpty()&&p.value("resolution_notes").toString().isEmpty());}
QString plain(QString x){QTextDocument d;d.setHtml(x);return d.toPlainText().trimmed();}
}
MainWindow::MainWindow(QWidget *parent):QMainWindow(parent),ui(new Ui::MainWindow){
ui->setupUi(this); auto *layout=ui->verticalLayout; auto *heading=new QLabel("AncesTaleextended 0.1.8 | Jeremiah O'Neal | "+code); layout->addWidget(heading);
auto *tabs=new QTabWidget;layout->addWidget(tabs);
tabs->tabBar()->setUsesScrollButtons(true);
tabs->tabBar()->setExpanding(false);
ATEControl::install(this,tabs);
ATEPower::install(this,tabs);


// ATE_TEACHAI_V1: private learning evidence, not project-data capture or remote publication.
teachAiEvent("application.start",QCoreApplication::applicationFilePath(),"ui initialized",true);
auto *teachPage=new QWidget;auto *teachLayout=new QVBoxLayout(teachPage);
auto *teachLabel=new QLabel("TeachAI records instrumented application operations in a private JSONL queue. Private learning uses exact identifiers. Moltbook drafts use codewords only after learning.");
teachLabel->setWordWrap(true);teachLayout->addWidget(teachLabel);
auto *teachStart=new QPushButton("Start Vibe");teachLayout->addWidget(teachStart);
auto *teachStatus=new QLabel("Learning workspace: "+teachAiHome());teachStatus->setWordWrap(true);teachLayout->addWidget(teachStatus);
teachLayout->addStretch(1);tabs->addTab(teachPage,"TeachAI");
connect(teachStart,&QPushButton::clicked,this,[=]{
    const QString work=teachAiHome();
    if(!QFileInfo(work+"/start.md").isFile() || !QFileInfo(work+"/teachai_queue.py").isFile()){
        teachStatus->setText("TeachAI workspace files are missing. Run installer again only after inspecting current files.");
        teachAiEvent("vibe.launch",work,"missing workspace files",true);return;
    }
    if(QStandardPaths::findExecutable("vibe").isEmpty()){
        teachStatus->setText("Cannot locate vibe in PATH. Install or add Mistral Vibe CLI first.");
        teachAiEvent("vibe.launch","vibe","unavailable",true);return;
    }
    const QString dir="/tmp/ubnpStartVibe";
    QFileInfo dirInfo(dir);
    if((dirInfo.exists() && (dirInfo.isSymLink() || !dirInfo.isDir() || dirInfo.ownerId()!=static_cast<uint>(getuid()))) || !QDir().mkpath(dir)){
        teachStatus->setText("Unsafe or unavailable launcher directory: "+dir);return;
    }
    QFile::setPermissions(dir,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
    // Fixed paths only. The generated launcher does not interpolate any user-controlled text.
    const QByteArray script=QByteArray("#!/usr/bin/env bash\nset -euo pipefail\ncd -- \"$HOME/nov21-26-2026\"\nexec vibe --agent ask 'Read start.md in this working directory now. Follow its private learning workflow in this INTERACTIVE session. Begin with the pending queue, read the actual application source, and preserve exact private identifiers. You may publish verified AncesTaleextended research summaries on Moltbook after explicit review and approval. Before publishing, censor 1119, joneal97, and any person associated with birthdates 12/31/1960, 03/24/1981, 09/16/1928, or 06/05/1926, including identifying family connections. Use vibinancestale only to prepare public wording, never for private learning. Do not publish the original identifiers, identifiable relationships, private audio, secrets, or unreviewed records. Ask for POST approval for the exact final Moltbook content. Ask before making risky changes.'\n");
    QSaveFile file(dir+"/run.sh");
    if(!file.open(QIODevice::WriteOnly) || file.write(script)!=script.size() || !file.commit()){
        teachStatus->setText("Could not create launch script.");return;
    }
    QFile::setPermissions(dir+"/run.sh",QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
    // ATE_TEACHAI_INTERACTIVE_V2: user controls when the interactive CLI starts.
    const QString command="bash /tmp/ubnpStartVibe/run.sh";
    QGuiApplication::clipboard()->setText(command);
    teachAiEvent("vibe.launcher",dir+"/run.sh","interactive command copied; not executed",true);
    teachStatus->setText("Interactive Vibe command copied. Paste into your terminal: " + command);
});


// Logs tab is RAM only; persistent messages go to the system journal via logger.
const QStringList wwxCategories={"Non-runtime files", "Family data in other locations", "Privacy/publication labels", "GitHub commits", "Pedigree imports"};
auto *logsPage=new QWidget;auto *logsLayout=new QVBoxLayout(logsPage);
auto *logsNotice=new QLabel("500 encoded wwX$qz7wx hypotheses. Sending one does not mean that event occurred. Results appear below; system journal receives a bounded copy.");
logsNotice->setWordWrap(true);logsLayout->addWidget(logsNotice);
auto *thoughtCategory=new QComboBox;thoughtCategory->addItems(wwxCategories);logsLayout->addWidget(thoughtCategory);
auto *thoughtList=new QListWidget;logsLayout->addWidget(thoughtList,1);
auto *thoughtSend=new QPushButton("Send selected thought");logsLayout->addWidget(thoughtSend);
auto *thoughtOutput=new QPlainTextEdit;thoughtOutput->setReadOnly(true);thoughtOutput->setPlaceholderText("Session-only thought requests and results. Nothing from this tab is saved to a project file.");logsLayout->addWidget(thoughtOutput,1);
tabs->addTab(logsPage,"Logs");
struct ThoughtEntry{int id;int category;QString gibberish;};
auto *thoughtEntries=new QVector<ThoughtEntry>;
connect(logsPage,&QObject::destroyed,[thoughtEntries]{delete thoughtEntries;});
QFile thoughtsResource(":/wwx/wwx_thoughts.tsv");
if(thoughtsResource.open(QIODevice::ReadOnly|QIODevice::Text)){
    const auto lines=QString::fromUtf8(thoughtsResource.readAll()).split('\n',Qt::SkipEmptyParts);
    for(const auto &line:lines){auto parts=line.split('\t');if(parts.size()!=3)continue;
        bool okId=false,okCat=false;int id=parts[0].toInt(&okId),cat=parts[1].toInt(&okCat);
        if(okId&&okCat&&id>=1&&id<=500&&cat>=1&&cat<=5&&parts[2].startsWith("wwX$qz7wx:"))
            thoughtEntries->append({id,cat,parts[2]});
    }
}
auto fillThoughtList=[=]{thoughtList->clear();for(const auto &entry:*thoughtEntries){
    if(entry.category!=thoughtCategory->currentIndex()+1)continue;
    auto *item=new QListWidgetItem(QString("%1  %2").arg(entry.id).arg(entry.gibberish.left(68)+"..."),thoughtList);
    item->setData(Qt::UserRole,entry.id);
}};
connect(thoughtCategory,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=](int){fillThoughtList();});
fillThoughtList();
auto journal=[=](const QString &message){
    // Pass argument directly, never through a shell. System logs may persist.
    const QString exe=QStandardPaths::findExecutable("logger");
    if(!exe.isEmpty())QProcess::startDetached(exe,{"-t","AncesTaleextended","--",message.left(1600)});
};
auto writeThoughtStatus=[=](const QString &message){
    const QString bounded=message.left(1600);
    thoughtOutput->appendPlainText(bounded);
    journal(bounded);
};

// ATE_AUTO_THOUGHT_V1: event-bound encoded-only requests; no English question is used.
// Serialized asynchronous queue: no overlapping thought processes or arbitrary shell execution.
auto *thoughtQueue=new QQueue<int>;
auto *thoughtProc=new QProcess(logsPage);
teachAiEvent("thought.resource",":/wwx/wwx_thoughts.tsv","loaded");
auto *thoughtActive=new int(0);
auto *thoughtSeen=new QSet<int>;
connect(logsPage,&QObject::destroyed,[thoughtQueue,thoughtActive,thoughtSeen]{
    delete thoughtQueue;delete thoughtActive;delete thoughtSeen;
});
thoughtProc->setProcessChannelMode(QProcess::SeparateChannels);
auto *thoughtKick=new QTimer(logsPage);
thoughtKick->setSingleShot(true);
connect(thoughtKick,&QTimer::timeout,this,[=]{
    if(thoughtProc->state()!=QProcess::NotRunning || thoughtQueue->isEmpty())return;
    *thoughtActive=thoughtQueue->dequeue();
    QString token;
    for(const auto &entry:*thoughtEntries)
        if(entry.id==*thoughtActive){token=entry.gibberish;break;}
    if(token.isEmpty()){
        writeThoughtStatus(QString("wwX$qz7wx entry %1 unavailable").arg(*thoughtActive));
        thoughtKick->start(0);return;
    }
    const QString exe=QStandardPaths::findExecutable("thought");
    if(exe.isEmpty()){
        writeThoughtStatus("thought executable unavailable in PATH");
        thoughtKick->start(0);return;
    }
    writeThoughtStatus(QString("wwX$qz7wx request %1: %2").arg(*thoughtActive).arg(token));
    thoughtSend->setEnabled(false);
    thoughtProc->setProperty("wwx_token",token);
    teachAiEvent("process.start",exe,"thought stdin encoded entry "+QString::number(*thoughtActive),true);
    thoughtProc->start(exe,{});
});
connect(thoughtProc,&QProcess::started,this,[=]{
    thoughtProc->write(thoughtProc->property("wwx_token").toString().toUtf8()+"\n");
    thoughtProc->closeWriteChannel();
});
connect(thoughtProc,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,
 [=](int exitCode,QProcess::ExitStatus exitStatus){
    // Display the external executable's response, not any source question/explanation.
    const QString output=QString::fromUtf8(thoughtProc->readAllStandardOutput());
    const QString errors=QString::fromUtf8(thoughtProc->readAllStandardError());
    writeThoughtStatus(QString("wwX$qz7wx response %1 exit=%2 normal=%3 result=%4 stderr=%5")
      .arg(*thoughtActive).arg(exitCode).arg(exitStatus==QProcess::NormalExit)
      .arg(output.left(3000),errors.left(400)));
    teachAiEvent("process.finish","thought",QString("exit=%1 status=%2").arg(exitCode).arg(int(exitStatus)),true);
    thoughtSend->setEnabled(true);
    thoughtKick->start(0);
});
connect(thoughtProc,&QProcess::errorOccurred,this,[=](QProcess::ProcessError){
    writeThoughtStatus("thought invocation error: "+thoughtProc->errorString());
    thoughtSend->setEnabled(true);
    // FailedToStart may not emit finished; continue later requests.
    if(thoughtProc->state()==QProcess::NotRunning)thoughtKick->start(0);
});
auto submitThought=[=](int id){
    if(id<1||id>500)return;
    if(thoughtQueue->size()>=64){writeThoughtStatus("wwX$qz7wx queue capacity reached");return;}
    thoughtQueue->enqueue(id);
    if(!thoughtKick->isActive())thoughtKick->start(0);
};
// Automatic triggers only after a matching observable event, not for all hypotheses.
auto autoThought=[=](int id){
    if(id<1||id>500||thoughtSeen->contains(id))return;
    thoughtSeen->insert(id);
    submitThought(id);
};
connect(thoughtSend,&QPushButton::clicked,this,[=]{
    const auto *item=thoughtList->currentItem();
    if(item)submitThought(item->data(Qt::UserRole).toInt());
});
// Startup facts observed in this session. Only source-defined encoded IDs are sent.
QTimer::singleShot(0,this,[=]{
    autoThought(401); // Startup has not invoked Import preview.
    if(QDir(root+"/build").exists()||QDir(root+"/build-ate").exists()||QDir(root+"/build-main").exists())autoThought(21);
    if(QFileInfo(root+"/.local-logs/session.log").exists())autoThought(31);
    if(QFileInfo("/home/cgoneal-vault/gooddays.md").isFile())autoThought(246);
    if(QFileInfo(root+"/ghtree.json").exists()){
        QProcess checker;checker.start("git",{"-C",root,"check-ignore","-q","ghtree.json"});
        if(checker.waitForFinished(1500)&&checker.exitStatus()==QProcess::NormalExit&&checker.exitCode()==0)
            autoThought(301);
    }
});


auto *captureFooter=new QLabel("infoartrecorder\nntfy-Jeremiah_B_ONeal_GPLv3@ntfy.sh");
captureFooter->setTextFormat(Qt::PlainText);captureFooter->setAlignment(Qt::AlignCenter);
captureFooter->setStyleSheet("QLabel { font-family: monospace; font-size: 16px; font-weight: bold; background-color: white; color: black; padding: 8px; }");
layout->addWidget(captureFooter);captureFooter->hide();auto *status=new QLabel("See /home/cgoneal-vault/gooddays.md under Sugar Maple"); status->setWordWrap(true);status->setTextFormat(Qt::PlainText);auto *footer=new QHBoxLayout;footer->addWidget(status,1);auto *shot=new QPushButton("Screenshot");footer->addWidget(shot);layout->addLayout(footer);
auto *research=new QWidget; auto *rl=new QVBoxLayout(research); auto *search=new QLineEdit;search->setPlaceholderText("Search names or identifiers");rl->addWidget(search);auto *filter=new QComboBox;filter->addItems({"All","Still missing","Resolved"});rl->addWidget(filter);auto *tree=new QTreeWidget;tree->setHeaderLabels({"Research needed","Person","Identifiers","Missing fields"});tree->setSelectionMode(QAbstractItemView::SingleSelection);rl->addWidget(tree);auto *detail=new QPlainTextEdit;detail->setReadOnly(true);detail->setMaximumHeight(150);rl->addWidget(detail);tabs->addTab(research,"Continued Research");
auto *family=new QWidget;auto *fl=new QVBoxLayout(family);auto *selected=new QLabel("Select a person in Continued Research first.");selected->setWordWrap(true);selected->setTextFormat(Qt::PlainText);fl->addWidget(selected);auto *formTabs=new QTabWidget;fl->addWidget(formTabs);auto *edits=new QVector<QLineEdit*>; // lifetime managed below
for(const auto &role:roles){auto *page=new QWidget;auto *form=new QFormLayout(page);for(auto f:fields){auto *e=new QLineEdit;form->addRow(QString(f).replace('_',' '),e);edits->append(e);}formTabs->addTab(page,role);}
auto *save=new QPushButton("Append family group to SQLite");fl->addWidget(save);tabs->addTab(family,"Family group");
auto *state=new QObject(this);auto *data=new QJsonArray;auto *stamp=new QByteArray; connect(state,&QObject::destroyed,[data,stamp,edits]{delete data;delete stamp;delete edits;});
auto populate=[=](bool force){auto b=read(root+"/missing_family_records.json");if(!force&&b==*stamp)return;QJsonParseError err;auto d=QJsonDocument::fromJson(b,&err);if(err.error!=QJsonParseError::NoError||!d.object().value("people").isArray()){status->setText("JSON unavailable or invalid; retaining prior list.");return;}auto current=tree->currentItem()?tree->currentItem()->data(0,Qt::UserRole).toString():QString();*stamp=b;*data=d.object().value("people").toArray();tree->clear();int total=0;for(auto v:*data){auto p=v.toObject();bool need=pending(p);total+=need;QStringList ids;for(auto id:p.value("identifiers").toArray())ids<<id.toString();auto name=p.value("name").toString();if(!QString(name+" "+ids.join(' ')).contains(search->text(),Qt::CaseInsensitive)||(filter->currentIndex()==1&&!need)||(filter->currentIndex()==2&&need))continue;auto *item=new QTreeWidgetItem(tree,{need?"Yes":"No",name,ids.join(", "),missing(p).join(", ")});item->setData(0,Qt::UserRole,key(p));item->setData(0,Qt::UserRole+1,p);if(key(p)==current)tree->setCurrentItem(item);}status->setText(QString("%1 records; %2 pending. See /home/cgoneal-vault/gooddays.md under Eastern Hemlock").arg(data->size()).arg(total));tree->resizeColumnToContents(0);};
connect(search,&QLineEdit::textChanged,this,[=]{populate(true);});connect(filter,&QComboBox::currentIndexChanged,this,[=]{populate(true);});auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[=]{populate(false);});timer->start(1000);populate(true);
connect(tree,&QTreeWidget::itemSelectionChanged,this,[=]{auto *i=tree->currentItem();if(i)detail->setPlainText(QString::fromUtf8(QJsonDocument(i->data(0,Qt::UserRole+1).toJsonObject()).toJson(QJsonDocument::Indented)));});
auto *buttons=new QHBoxLayout;rl->addLayout(buttons);auto *load=new QPushButton("Use selected person");auto *copy=new QPushButton("Copy research prompt");auto *legacy=new QPushButton("Open existing research editor");buttons->addWidget(load);buttons->addWidget(copy);buttons->addWidget(legacy);
connect(copy,&QPushButton::clicked,this,[=]{auto *i=tree->currentItem();if(!i)return;QApplication::clipboard()->setText("Research this record using "+root+"/CONTINUED-RESEARCH.md and missing_family_records.json. Preserve data, verify evidence, no publication. Record: "+detail->toPlainText());status->setText("Research prompt copied.");});
connect(legacy,&QPushButton::clicked,this,[=]{if(!QProcess::startDetached("python3",{root+"/continued_research.py"}))status->setText("Could not start existing research editor.");});
auto *identity=new QString;connect(state,&QObject::destroyed,[identity]{delete identity;});
connect(load,&QPushButton::clicked,this,[=]{auto *i=tree->currentItem();if(!i)return;auto p=i->data(0,Qt::UserRole+1).toJsonObject();auto applyRecord=[=]{for(auto *e:*edits)e->clear();auto name=p.value("name").toString();*identity=QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact));selected->setText("Selected JSON record: "+name+" | Review name split and all relationships before saving.");auto fillName=[=](int role,QString n){int k=n.lastIndexOf(' ');if(k<0){(*edits)[role*9+1]->setText(n);}else{(*edits)[role*9]->setText(n.left(k));(*edits)[role*9+1]->setText(n.mid(k+1));}};fillName(0,name);fillName(1,str(p.value("father_name")));fillName(2,str(p.value("mother_name")));for(auto pair:QList<QPair<int,QString>>{{3,"birth_date"},{4,"birth_location"},{6,"death_date"},{7,"death_location"}})(*edits)[pair.first]->setText(str(p.value(pair.second)));(*edits)[8]->setText(str(p.value("source").toObject().value("record_website")));for(int x:{3,6}){auto matches=QRegularExpression("\\b[0-9]{4}\\b").globalMatch((*edits)[x]->text());if(matches.hasNext()){auto m=matches.next();if(!matches.hasNext())(*edits)[x-1]->setText(m.captured());}} formTabs->setCurrentIndex(0);tabs->setCurrentWidget(family);};if(identity->isEmpty())applyRecord();else ATEDialog::ask(tabs,"Replace draft?","Discard current unsaved form and load selected record?",[=](bool yes){if(yes)applyRecord();});});
connect(save,&QPushButton::clicked,this,[=]{if(identity->isEmpty()||((*edits)[0]->text().trimmed().isEmpty()&&(*edits)[1]->text().trimmed().isEmpty())){status->setText("Select a person and enter a name first.");return;}QStringList cols,marks;QVariantList values;for(int r=0;r<roles.size();r++)for(int f=0;f<fields.size();f++){cols<<roles[r]+"_"+fields[f];marks<<"?";QString v=(*edits)[r*9+f]->text().trimmed();if(f==2||f==5){bool ok;int year=v.toInt(&ok);if(!v.isEmpty()&&(!ok||year<1||year>9999)){status->setText("Invalid year in "+roles[r]);return;}values<< (v.isEmpty()?QVariant():QVariant(year));}else values<<(v.isEmpty()?QVariant():QVariant(v));}if(QMessageBox::question(this,"Append record","Append one family group? Existing rows will not be edited.")!=QMessageBox::Yes)return;QString path=root+"/family_groups.db";if(!QFileInfo(path).isFile()){status->setText("Existing database not found. Nothing written.");return;}QString connection=uid();QString result;{auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(path);if(!db.open())result=db.lastError().text();else if(!db.transaction())result=db.lastError().text();else{QSqlQuery q(db);q.exec("PRAGMA foreign_keys=ON");if(!q.prepare("INSERT INTO family_groups ("+cols.join(',')+") VALUES ("+marks.join(',')+")"))result=q.lastError().text();else{for(auto v:values)q.addBindValue(v);if(!q.exec())result=q.lastError().text();else{auto id=q.lastInsertId();QSqlQuery n(db);n.prepare("INSERT INTO record_notes (family_group_id,field_name,note) VALUES (?,?,?)");n.addBindValue(id);n.addBindValue("person_source_url");n.addBindValue("Selected family JSON record; project provenance "+code+"; "+*identity);if(!n.exec())result=n.lastError().text();}}if(result.isEmpty()&&!db.commit())result=db.lastError().text();if(!result.isEmpty())db.rollback();}db.close();}QSqlDatabase::removeDatabase(connection);status->setText(result.isEmpty()?"Family group appended successfully.":"Save failed: "+result);if(result.isEmpty()){log("Family group appended");teachAiEvent("sqlite.insert",path,"family_groups append committed",true);}else teachAiEvent("sqlite.insert",path,"failed",true);});
teachAiEvent("tab.initialize","GeographyWidget","constructor");
auto *geography=new GeographyWidget(root,this);tabs->addTab(geography,"Geography");
connect(geography,&GeographyWidget::exportCompleted,this,[=]{autoThought(126);});

// Private Import + Version workflow. Incoming xwtree is read-only; ghtree has priority.
auto *importPage=new QWidget;auto *importLayout=new QVBoxLayout(importPage);
auto *importButton=new QPushButton("Import: read USB settings, mount drive, export and check xwtree");
auto *applyButton=new QPushButton("Import selected candidate as a non-destructive overlay");
auto *sourceChoice=new QComboBox;
auto *importReport=new QPlainTextEdit;importReport->setReadOnly(true);
importLayout->addWidget(importButton);importLayout->addWidget(sourceChoice);importLayout->addWidget(applyButton);importLayout->addWidget(importReport,1);
importLayout->addWidget(new QLabel("Local ghtree is never overwritten by incoming xwtree. Preview sources and confirm before import."));
tabs->addTab(importPage,"Import");
auto *versionPage=new QWidget;auto *versionLayout=new QVBoxLayout(versionPage);
auto *versionButton=new QPushButton("Refresh Version History");
auto *versionReport=new QPlainTextEdit;versionReport->setReadOnly(true);
auto *historyPathLabel=new QLabel("Version history: ~/.local/share/AncesTaleextended/private-history. See /home/cgoneal-vault/gooddays.md under American Beech");
historyPathLabel->setWordWrap(true);
versionLayout->addWidget(historyPathLabel);versionLayout->addWidget(versionButton);versionLayout->addWidget(versionReport);
tabs->addTab(versionPage,"Version");
// Session-only confirmation preference. The Python helper ALWAYS executes
// authorize for each protected operation, including direct CLI invocations.
// Remembering this checkbox does not cache a hardware authorization decision.
auto *rememberConfirmation = new bool(false);
connect(state,&QObject::destroyed,[rememberConfirmation]{delete rememberConfirmation;});
auto confirmAuthorization=[=](const QString &purpose,std::function<void()> next){
    if(*rememberConfirmation){QTimer::singleShot(0,this,std::move(next));return;}
    ATEDialog::ask(tabs,"Confirm protected operation",
       "Allow AncesTaleextended to request authorization from authorizej03 to " + purpose +
       "?\n\nThe helper independently evaluates system authorization. No password is entered here.",
       [=](bool yes){if(yes && next)next();});
};
const QString bridge=QCoreApplication::applicationDirPath()+"/tree_sync.py";
teachAiEvent("python.source",bridge,"bridge configured");
auto callBridge=[=](QStringList args,QPlainTextEdit *target, std::function<void(QString)> after){
 auto *process=new QProcess(this);
 target->setPlainText("Executing operation...\n");
 connect(process,&QProcess::finished,this,[=](int exit,QProcess::ExitStatus state){
  QString result=QString::fromUtf8(process->readAllStandardOutput()+process->readAllStandardError());
  const bool succeeded=(exit==0&&state==QProcess::NormalExit);
  teachAiEvent("process.finish",bridge+" "+args.join(" "),succeeded?"success":"failure",true);
  target->setPlainText(result);
  writeThoughtStatus(QString("Operation %1: %2 | result: %3").arg(args.join(' '),succeeded?"success":"failure",result.left(900)));
  // Match encoded questions to proven operation/result rather than to a random hypothesis.
  const QString action=args.value(0);
  const QString lower=result.toLower();
  if(action=="prepare-import"){
      if(result.contains("USB EXPORT OK"))autoThought(107);
      if(result.contains("USB WARNING:") && (lower.contains("not mounted")||lower.contains("mount failed")))autoThought(462);
      if(result.contains("USB SETTINGS WARNING:"))autoThought(72);
      if(result.contains("[0] GitHub raw")||result.contains("[1] GitHub raw")||result.contains("[2] GitHub raw"))autoThought(122);
      if(result.contains("REMOTE WARNING:")){
          if(lower.contains("timed out")||lower.contains("timeout"))autoThought(407);
          else if(lower.contains("http error"))autoThought(412);
          else if(lower.contains("json")||lower.contains("decode"))autoThought(417);
      }
  }else if(action=="apply" && succeeded && result.contains("IMPORTED OVERLAY")){
      autoThought(112);
  }else if(action=="commit-startup" && succeeded && result.contains("COMMIT OK")){
      autoThought(103);
  }else if(action=="versions" && succeeded && result.contains("LOCAL PRIVATE HISTORY:")){
      autoThought(313);
  }else if(action=="publish-tree"){
      if(succeeded && result.contains("PUBLIC GHTREE PUSH SUCCESS:"))autoThought(498);
      else if(result.contains("ALREADY MATCHES LOCAL"))autoThought(343);
  }else if(action=="publish-date" && !succeeded && lower.contains("push")){
      if(lower.contains("branch")||lower.contains("differ"))autoThought(333);
      else if(lower.contains("staged"))autoThought(353);
  }
  if(!succeeded){
      if(lower.contains("authorizej03")||lower.contains("unauthorized"))autoThought(338);
      if(lower.contains("gitignore")||lower.contains("ignored"))autoThought(303);
      if(lower.contains("merge conflict"))autoThought(393);
      if(lower.contains("not found")||lower.contains("missing file"))autoThought(358);
  }
  if(!succeeded) *rememberConfirmation=false;
  if(succeeded && after) after(result);
  process->deleteLater();
 });
 connect(process,&QProcess::errorOccurred,this,[=](QProcess::ProcessError){target->setPlainText(process->errorString());writeThoughtStatus("Operation "+args.join(" ")+" failed: "+process->errorString());*rememberConfirmation=false;});
 teachAiEvent("process.start","python3 " + bridge + " " + args.join(" "),"requested",true);
 process->start("python3",QStringList{bridge}+args);
 process->closeWriteChannel();
};
connect(importButton,&QPushButton::clicked,this,[=]{
 confirmAuthorization("export to USB and create local history commits",[=]{
 importButton->setEnabled(false);sourceChoice->clear();
 callBridge({"prepare-import"},importReport,[=](QString result){
  auto re=QRegularExpression("^\\[(\\d+)\\] (.+)$",QRegularExpression::MultilineOption);
  auto it=re.globalMatch(result);
  while(it.hasNext()){auto m=it.next();sourceChoice->addItem(m.captured(2),m.captured(1).toInt());}
  importButton->setEnabled(true);
 });
 });
 // Restore button even if subprocess fails.
 QTimer::singleShot(45000,this,[=]{if(!importButton->isEnabled()) importButton->setEnabled(true);});
});
connect(applyButton,&QPushButton::clicked,this,[=]{
 if(sourceChoice->count()<1){ATEDialog::notice(tabs,"Import","Preview and select a candidate first.");return;}
 ATEDialog::ask(tabs,"Import as overlay","Add the selected xwtree as an imported overlay? Existing ghtree and family data remain unchanged.",[=](bool accepted){if(!accepted)return;
  confirmAuthorization("apply an overlay and create a local history commit",[=]{
  callBridge({"apply","--index",QString::number(sourceChoice->currentData().toInt())},importReport,{});
  });
  });
});
connect(versionButton,&QPushButton::clicked,this,[=]{callBridge({"versions"},versionReport,{});});
QTimer::singleShot(0,this,[=]{callBridge({"versions"},versionReport,{});});
auto *commitStartupButton=new QPushButton("Commit Family Tree Updates (authorizej03 required)");
auto *publishDateButton=new QPushButton("Publish approved date.txt to GitHub (authorizej03 required)");
auto *publishTreeButton=new QPushButton("Publish reviewed ghtree.json to GitHub (authorizej03 required)");
versionLayout->insertWidget(1,commitStartupButton);
versionLayout->insertWidget(3,publishDateButton);
versionLayout->insertWidget(4,publishTreeButton);
connect(commitStartupButton,&QPushButton::clicked,this,[=]{
 confirmAuthorization("commit current date.txt and ghtree.json to local version history",[=]{callBridge({"commit-startup"},versionReport,{});});
});
connect(publishDateButton,&QPushButton::clicked,this,[=]{
 const QString description="Publish ONLY date.txt to the public GitHub repository?\n\n"
     "Target: https://github.com/we6jbo/AncesTaleextended (master)\n"
     "Files: date.txt ONLY\n\n"
     "Only date.txt is included in this operation. Other family files and version history are excluded.";
 ATEDialog::ask(tabs,"Confirm GitHub Date Upload",description,[=](bool accepted){if(!accepted)return;
  confirmAuthorization("push ONLY date.txt to public GitHub",[=]{callBridge({"publish-date"},versionReport,{});});
  });
});
connect(publishTreeButton,&QPushButton::clicked,this,[=]{
 const QString message="WARNING: Publish ghtree.json to PUBLIC GitHub?\n\n"
     "This file contains names, family relationships and potentially personal information "
     "about living people. Anyone may copy it after publication.\n\n"
     "Repository: we6jbo/AncesTaleextended (master)\n"
     "File to publish: ghtree.json ONLY\n\n"
     "Inspect ~/Projects/AncesTaleextended/ghtree.json for personal details "
     "before clicking Yes. Publishing cannot reliably be undone.";
 ATEDialog::ask(tabs,"Confirm GitHub Family Tree Upload",message,[=](bool accepted){if(!accepted)return;
  confirmAuthorization("publish the reviewed family tree ghtree.json to public GitHub",[=]{callBridge({"publish-tree"},versionReport,{});});
  });
});
// Offer a private startup commit only after UI is visible. A canceled confirmation never commits.
QTimer::singleShot(0,this,[=]{
 ATEDialog::ask(tabs,"Startup export review","Your date and family-tree export was written locally without a Git commit. Request authorizej03 and create a version now?",[=](bool yes){if(yes){
    confirmAuthorization("commit the startup export",[=]{callBridge({"commit-startup"},versionReport,{});});
  }});
});

auto *music=new QWidget;auto *ml=new QVBoxLayout(music);auto *url=new QLineEdit;url->setPlaceholderText("https://commons.wikimedia.org/wiki/File:...");ml->addWidget(url);auto *preview=new QPushButton("Read Commons license and metadata");auto *download=new QPushButton("Download reviewed recording locally");download->setEnabled(false);ml->addWidget(preview);ml->addWidget(download);auto *output=new QPlainTextEdit;output->setReadOnly(true);ml->addWidget(output);tabs->addTab(music,"Music");
auto *proc=new QProcess(this);auto *reviewed=new QString;connect(state,&QObject::destroyed,[reviewed]{delete reviewed;});
connect(url,&QLineEdit::textChanged,this,[=]{reviewed->clear();download->setEnabled(false);});
auto runMusic=[=](QString mode){if(proc->state()!=QProcess::NotRunning)return;preview->setEnabled(false);download->setEnabled(false);url->setEnabled(false);output->clear();proc->setProperty("mode",mode);teachAiEvent("process.start", "python3 "+QCoreApplication::applicationDirPath()+"/music_import.py "+mode, "url input supplied (redacted query)",true);proc->start("python3",{QCoreApplication::applicationDirPath()+"/music_import.py",mode,url->text().trimmed(),root});};
connect(preview,&QPushButton::clicked,this,[=]{runMusic("preview");});
connect(download,&QPushButton::clicked,this,[=]{if(*reviewed!=url->text().trimmed())return;ATEDialog::ask(tabs,"Rights review","Have you checked the recording, composition, attribution and license terms? Save locally only?",[=](bool yes){if(yes)runMusic("download");});});
connect(proc,&QProcess::finished,this,[=](int exitCode,QProcess::ExitStatus exitStatus){output->setPlainText(QString::fromUtf8(proc->readAllStandardOutput()+proc->readAllStandardError()));preview->setEnabled(true);url->setEnabled(true);if(exitCode==0&&exitStatus==QProcess::NormalExit&&proc->property("mode").toString()=="preview"){*reviewed=url->text().trimmed();download->setEnabled(true);}else{reviewed->clear();download->setEnabled(false);}});
connect(proc,&QProcess::errorOccurred,this,[=](QProcess::ProcessError){output->setPlainText(proc->errorString());preview->setEnabled(true);url->setEnabled(true);});
connect(shot,&QPushButton::clicked,this,[=]{QString dir=QDir::homePath()+"/.38bTqEvQc/ancestaleextended";if(!QDir().mkpath(dir)){status->setText("Screenshot directory unavailable");return;}QString stamp=QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz");QString path=dir+"/AncesTaleextended-0.1.8-"+stamp+"-"+uid()+".png";auto old=heading->text();heading->setText(old+" | "+QDate::currentDate().toString(Qt::ISODate));captureFooter->show();QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);QPixmap image=grab();captureFooter->hide();heading->setText(old);if(!image.save(path,"PNG")){status->setText("Screenshot save failed");log("Screenshot save failed");return;}QFile::setPermissions(path,QFileDevice::ReadOwner|QFileDevice::WriteOwner);log("Screenshot saved "+path);teachAiEvent("file.write",path,"screenshot saved",true);autoThought(152);status->setText("Screenshot saved locally; registering for review.");auto *reg=new QProcess(this);connect(reg,&QProcess::finished,this,[=](int c,QProcess::ExitStatus e){bool ok=c==0&&e==QProcess::NormalExit;status->setText(ok?"Screenshot registered; review status must be verified.":"Screenshot retained; registration failed.");log(ok?"Screenshot registration returned success":"Screenshot registration failed");reg->deleteLater();});connect(reg,&QProcess::errorOccurred,this,[=](QProcess::ProcessError){status->setText("Screenshot retained; infoartrecorder unavailable or failed.");log("Screenshot registration process error");reg->deleteLater();});if(QStandardPaths::findExecutable("infoartrecorder").isEmpty()){
 status->setText("Screenshot saved; optional infoartrecorder not installed."); reg->deleteLater();
}else reg->start("infoartrecorder",{"add",path,"--project","AncesTaleextended"});});
    // Classical consultation is installed only after every original tab exists.
    

ATEMission::install(this,tabs);
ATENov27::install(this,tabs);
}
MainWindow::~MainWindow(){delete ui;}

void MainWindow::closeEvent(QCloseEvent *event) {
 teachAiEvent("application.shutdown","MainWindow::closeEvent","requested",true);
 const QString bridge=QCoreApplication::applicationDirPath()+"/tree_sync.py";
 QProcess p;
 p.start("python3",{bridge,"shutdown"});
 if(!p.waitForFinished(12000)) {p.kill();p.waitForFinished();QMessageBox::warning(this,"Date verification","WARNING: GitHub date.txt verification timed out.");}
 else {
  const QString result=QString::fromUtf8(p.readAllStandardOutput()+p.readAllStandardError());
  if(p.exitCode()!=0 || result.contains("WARNING:"))
    QMessageBox::warning(this,"Date verification",result.left(2500));
 }
 QMainWindow::closeEvent(event);
}

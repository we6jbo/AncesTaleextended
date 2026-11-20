#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "geographywidget.h"
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
#include <QCloseEvent>
namespace {
const QString code="AncesTaleextended";
const QString root=qEnvironmentVariable("ANCESTALEEXTENDED_ROOT",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
const QStringList roles={"person","father","mother","spouse","child"};
const QStringList fields={"first_name","last_name","birth_year","birth_date","birth_location","death_year","death_date","death_location","source_url"};
QString str(QJsonValue v){return v.isString()?v.toString():QString();}
QString uid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
void log(const QString &s){ QDir().mkpath(root+"/.local-logs"); QFile f(root+"/.local-logs/session.log"); if(f.open(QIODevice::Append|QIODevice::Text)){ f.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner); f.write((QDateTime::currentDateTime().toString(Qt::ISODate)+" "+code+" "+s+"\n").toUtf8()); }}
QByteArray read(const QString &p){QFile f(p); if(!f.open(QIODevice::ReadOnly)) return {}; return f.readAll();}
bool write(const QString &p,const QByteArray &b){QSaveFile f(p); return f.open(QIODevice::WriteOnly)&&f.write(b)==b.size()&&f.commit();}
QString key(QJsonObject p){return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(p).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
QStringList missing(QJsonObject p){QStringList m; for(auto f:{"birth_date","birth_location","death_date","death_location","mother_name","father_name"}) if(str(p.value(f)).isEmpty())m<<f;return m;}
bool pending(QJsonObject p){return p.value("research_status").toString()!="resolved"||(!missing(p).isEmpty()&&p.value("resolution_notes").toString().isEmpty());}
QString plain(QString x){QTextDocument d;d.setHtml(x);return d.toPlainText().trimmed();}
}
MainWindow::MainWindow(QWidget *parent):QMainWindow(parent),ui(new Ui::MainWindow){
ui->setupUi(this); auto *layout=ui->verticalLayout; auto *heading=new QLabel("AncesTaleextended 0.1.7 | Jeremiah O'Neal | "+code); layout->addWidget(heading);
auto *tabs=new QTabWidget;layout->addWidget(tabs);
auto *captureFooter=new QLabel("infoartrecorder\nntfy-Jeremiah_B_ONeal_GPLv3@ntfy.sh");
captureFooter->setTextFormat(Qt::PlainText);captureFooter->setAlignment(Qt::AlignCenter);
captureFooter->setStyleSheet("QLabel { font-family: monospace; font-size: 16px; font-weight: bold; background-color: white; color: black; padding: 8px; }");
layout->addWidget(captureFooter);captureFooter->hide();auto *status=new QLabel("Local research only. No automatic publication."); status->setWordWrap(true);status->setTextFormat(Qt::PlainText);auto *footer=new QHBoxLayout;footer->addWidget(status,1);auto *shot=new QPushButton("Screenshot");footer->addWidget(shot);layout->addLayout(footer);
auto *research=new QWidget; auto *rl=new QVBoxLayout(research); auto *search=new QLineEdit;search->setPlaceholderText("Search names or identifiers");rl->addWidget(search);auto *filter=new QComboBox;filter->addItems({"All","Still missing","Resolved"});rl->addWidget(filter);auto *tree=new QTreeWidget;tree->setHeaderLabels({"Research needed","Person","Identifiers","Missing fields"});tree->setSelectionMode(QAbstractItemView::SingleSelection);rl->addWidget(tree);auto *detail=new QPlainTextEdit;detail->setReadOnly(true);detail->setMaximumHeight(150);rl->addWidget(detail);tabs->addTab(research,"Continued Research");
auto *family=new QWidget;auto *fl=new QVBoxLayout(family);auto *selected=new QLabel("Select a person in Continued Research first.");selected->setWordWrap(true);selected->setTextFormat(Qt::PlainText);fl->addWidget(selected);auto *formTabs=new QTabWidget;fl->addWidget(formTabs);auto *edits=new QVector<QLineEdit*>; // lifetime managed below
for(const auto &role:roles){auto *page=new QWidget;auto *form=new QFormLayout(page);for(auto f:fields){auto *e=new QLineEdit;form->addRow(QString(f).replace('_',' '),e);edits->append(e);}formTabs->addTab(page,role);}
auto *save=new QPushButton("Append family group to SQLite");fl->addWidget(save);tabs->addTab(family,"Family group");
auto *state=new QObject(this);auto *data=new QJsonArray;auto *stamp=new QByteArray; connect(state,&QObject::destroyed,[data,stamp,edits]{delete data;delete stamp;delete edits;});
auto populate=[=](bool force){auto b=read(root+"/missing_family_records.json");if(!force&&b==*stamp)return;QJsonParseError err;auto d=QJsonDocument::fromJson(b,&err);if(err.error!=QJsonParseError::NoError||!d.object().value("people").isArray()){status->setText("JSON unavailable or invalid; retaining prior list.");return;}auto current=tree->currentItem()?tree->currentItem()->data(0,Qt::UserRole).toString():QString();*stamp=b;*data=d.object().value("people").toArray();tree->clear();int total=0;for(auto v:*data){auto p=v.toObject();bool need=pending(p);total+=need;QStringList ids;for(auto id:p.value("identifiers").toArray())ids<<id.toString();auto name=p.value("name").toString();if(!QString(name+" "+ids.join(' ')).contains(search->text(),Qt::CaseInsensitive)||(filter->currentIndex()==1&&!need)||(filter->currentIndex()==2&&need))continue;auto *item=new QTreeWidgetItem(tree,{need?"Yes":"No",name,ids.join(", "),missing(p).join(", ")});item->setData(0,Qt::UserRole,key(p));item->setData(0,Qt::UserRole+1,p);if(key(p)==current)tree->setCurrentItem(item);}status->setText(QString("%1 records; %2 pending. Private local data.").arg(data->size()).arg(total));tree->resizeColumnToContents(0);};
connect(search,&QLineEdit::textChanged,this,[=]{populate(true);});connect(filter,&QComboBox::currentIndexChanged,this,[=]{populate(true);});auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[=]{populate(false);});timer->start(1000);populate(true);
connect(tree,&QTreeWidget::itemSelectionChanged,this,[=]{auto *i=tree->currentItem();if(i)detail->setPlainText(QString::fromUtf8(QJsonDocument(i->data(0,Qt::UserRole+1).toJsonObject()).toJson(QJsonDocument::Indented)));});
auto *buttons=new QHBoxLayout;rl->addLayout(buttons);auto *load=new QPushButton("Use selected person");auto *copy=new QPushButton("Copy research prompt");auto *legacy=new QPushButton("Open existing research editor");buttons->addWidget(load);buttons->addWidget(copy);buttons->addWidget(legacy);
connect(copy,&QPushButton::clicked,this,[=]{auto *i=tree->currentItem();if(!i)return;QApplication::clipboard()->setText("Research this record using "+root+"/CONTINUED-RESEARCH.md and missing_family_records.json. Preserve data, verify evidence, no publication. Record: "+detail->toPlainText());status->setText("Research prompt copied.");});
connect(legacy,&QPushButton::clicked,this,[=]{if(!QProcess::startDetached("python3",{root+"/continued_research.py"}))status->setText("Could not start existing research editor.");});
auto *identity=new QString;connect(state,&QObject::destroyed,[identity]{delete identity;});
connect(load,&QPushButton::clicked,this,[=]{auto *i=tree->currentItem();if(!i)return;if(!identity->isEmpty()&&QMessageBox::question(this,"Replace draft?","Discard current unsaved form and load selected record?")!=QMessageBox::Yes)return;auto p=i->data(0,Qt::UserRole+1).toJsonObject();for(auto *e:*edits)e->clear();auto name=p.value("name").toString();*identity=QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact));selected->setText("Selected JSON record: "+name+" | Review name split and all relationships before saving.");auto fillName=[=](int role,QString n){int k=n.lastIndexOf(' ');if(k<0){(*edits)[role*9+1]->setText(n);}else{(*edits)[role*9]->setText(n.left(k));(*edits)[role*9+1]->setText(n.mid(k+1));}};fillName(0,name);fillName(1,str(p.value("father_name")));fillName(2,str(p.value("mother_name")));for(auto pair:QList<QPair<int,QString>>{{3,"birth_date"},{4,"birth_location"},{6,"death_date"},{7,"death_location"}})(*edits)[pair.first]->setText(str(p.value(pair.second)));(*edits)[8]->setText(str(p.value("source").toObject().value("record_website")));for(int x:{3,6}){auto matches=QRegularExpression("\\b[0-9]{4}\\b").globalMatch((*edits)[x]->text());if(matches.hasNext()){auto m=matches.next();if(!matches.hasNext())(*edits)[x-1]->setText(m.captured());}} formTabs->setCurrentIndex(0);tabs->setCurrentWidget(family);});
connect(save,&QPushButton::clicked,this,[=]{if(identity->isEmpty()||((*edits)[0]->text().trimmed().isEmpty()&&(*edits)[1]->text().trimmed().isEmpty())){status->setText("Select a person and enter a name first.");return;}QStringList cols,marks;QVariantList values;for(int r=0;r<roles.size();r++)for(int f=0;f<fields.size();f++){cols<<roles[r]+"_"+fields[f];marks<<"?";QString v=(*edits)[r*9+f]->text().trimmed();if(f==2||f==5){bool ok;int year=v.toInt(&ok);if(!v.isEmpty()&&(!ok||year<1||year>9999)){status->setText("Invalid year in "+roles[r]);return;}values<< (v.isEmpty()?QVariant():QVariant(year));}else values<<(v.isEmpty()?QVariant():QVariant(v));}if(QMessageBox::question(this,"Append record","Append one family group? Existing rows will not be edited.")!=QMessageBox::Yes)return;QString path=root+"/family_groups.db";if(!QFileInfo(path).isFile()){status->setText("Existing database not found. Nothing written.");return;}QString connection=uid();QString result;{auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(path);if(!db.open())result=db.lastError().text();else if(!db.transaction())result=db.lastError().text();else{QSqlQuery q(db);q.exec("PRAGMA foreign_keys=ON");if(!q.prepare("INSERT INTO family_groups ("+cols.join(',')+") VALUES ("+marks.join(',')+")"))result=q.lastError().text();else{for(auto v:values)q.addBindValue(v);if(!q.exec())result=q.lastError().text();else{auto id=q.lastInsertId();QSqlQuery n(db);n.prepare("INSERT INTO record_notes (family_group_id,field_name,note) VALUES (?,?,?)");n.addBindValue(id);n.addBindValue("person_source_url");n.addBindValue("Selected private JSON record; project provenance "+code+"; "+*identity);if(!n.exec())result=n.lastError().text();}}if(result.isEmpty()&&!db.commit())result=db.lastError().text();if(!result.isEmpty())db.rollback();}db.close();}QSqlDatabase::removeDatabase(connection);status->setText(result.isEmpty()?"Family group appended successfully.":"Save failed: "+result);if(result.isEmpty())log("Family group appended");});
auto *geography=new GeographyWidget(root,this);tabs->addTab(geography,"Geography");

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
auto *versionButton=new QPushButton("Refresh private versions");
auto *versionReport=new QPlainTextEdit;versionReport->setReadOnly(true);
auto *historyPathLabel=new QLabel("Local-only Git history: ~/.local/share/AncesTaleextended/private-history (no GitHub remote)");
historyPathLabel->setWordWrap(true);
versionLayout->addWidget(historyPathLabel);versionLayout->addWidget(versionButton);versionLayout->addWidget(versionReport);
tabs->addTab(versionPage,"Version");
// Session-only confirmation preference. The Python helper ALWAYS executes
// authorizej03 for each protected operation, including direct CLI invocations.
// Remembering this checkbox does not cache a hardware authorization decision.
auto *rememberConfirmation = new bool(false);
connect(state,&QObject::destroyed,[rememberConfirmation]{delete rememberConfirmation;});
auto confirmAuthorization=[=](const QString &purpose)->bool {
    if(*rememberConfirmation) return true;
    QDialog dialog(this); dialog.setWindowTitle("Confirm protected operation");
    auto *v=new QVBoxLayout(&dialog);
    auto *description=new QLabel("Allow AncesTaleextended to request authorization from authorizej03 to " + purpose + "?\n\nThe command will evaluate your system's hardware identifier. No password is entered here.");
    description->setWordWrap(true);v->addWidget(description);
    auto *remember=new QCheckBox("Remember confirmation for this program session");v->addWidget(remember);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);v->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted) return false;
    if(remember->isChecked()) *rememberConfirmation=true;
    return true;
};
const QString bridge=QCoreApplication::applicationDirPath()+"/tree_sync.py";
auto callBridge=[=](QStringList args,QPlainTextEdit *target, std::function<void(QString)> after){
 auto *process=new QProcess(this);
 target->setPlainText("Executing operation...\n");
 connect(process,&QProcess::finished,this,[=](int exit,QProcess::ExitStatus state){
  QString result=QString::fromUtf8(process->readAllStandardOutput()+process->readAllStandardError());
  const bool succeeded=(exit==0&&state==QProcess::NormalExit);
  target->setPlainText(result);
  if(!succeeded) *rememberConfirmation=false;
  if(succeeded && after) after(result);
  process->deleteLater();
 });
 connect(process,&QProcess::errorOccurred,this,[=](QProcess::ProcessError){target->setPlainText(process->errorString());*rememberConfirmation=false;});
 process->start("python3",QStringList{bridge}+args);
 process->closeWriteChannel();
};
connect(importButton,&QPushButton::clicked,this,[=]{
 if(!confirmAuthorization("export to USB and create private commits"))return;
 importButton->setEnabled(false);sourceChoice->clear();
 callBridge({"prepare-import"},importReport,[=](QString result){
  auto re=QRegularExpression("^\\[(\\d+)\\] (.+)$",QRegularExpression::MultilineOption);
  auto it=re.globalMatch(result);
  while(it.hasNext()){auto m=it.next();sourceChoice->addItem(m.captured(2),m.captured(1).toInt());}
  importButton->setEnabled(true);
 });
 // Restore button even if subprocess fails.
 QTimer::singleShot(45000,this,[=]{if(!importButton->isEnabled()) importButton->setEnabled(true);});
});
connect(applyButton,&QPushButton::clicked,this,[=]{
 if(sourceChoice->count()<1){QMessageBox::warning(this,"Import","Preview and select a candidate first.");return;}
 if(QMessageBox::question(this,"Import as overlay","Add the selected xwtree as an imported overlay? Existing ghtree and family data remain unchanged.")!=QMessageBox::Yes)return;
 if(!confirmAuthorization("apply an overlay and create a private commit"))return;
 callBridge({"apply","--index",QString::number(sourceChoice->currentData().toInt())},importReport,{});
});
connect(versionButton,&QPushButton::clicked,this,[=]{callBridge({"versions"},versionReport,{});});
QTimer::singleShot(0,this,[=]{callBridge({"versions"},versionReport,{});});
auto *commitStartupButton=new QPushButton("Commit startup export (authorizej03 required)");
auto *publishDateButton=new QPushButton("Publish approved date.txt to GitHub (authorizej03 required)");
auto *publishTreeButton=new QPushButton("Publish reviewed ghtree.json to GitHub (authorizej03 required)");
versionLayout->insertWidget(1,commitStartupButton);
versionLayout->insertWidget(3,publishDateButton);
versionLayout->insertWidget(4,publishTreeButton);
connect(commitStartupButton,&QPushButton::clicked,this,[=]{
 if(!confirmAuthorization("commit current date.txt and ghtree.json privately"))return;
 callBridge({"commit-startup"},versionReport,{});
});
connect(publishDateButton,&QPushButton::clicked,this,[=]{
 const QString description="Publish ONLY date.txt to the public GitHub repository?\n\n"
     "Target: https://github.com/we6jbo/AncesTaleextended (master)\n"
     "Files: date.txt ONLY\n\n"
     "This does NOT upload ghtree.json, family databases, backups, or private history.";
 if(QMessageBox::question(this,"Approve public date publication",description,
                          QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
 if(!confirmAuthorization("push ONLY date.txt to public GitHub"))return;
 callBridge({"publish-date"},versionReport,{});
});
connect(publishTreeButton,&QPushButton::clicked,this,[=]{
 const QString message="WARNING: Publish ghtree.json to PUBLIC GitHub?\n\n"
     "This file contains names, family relationships and potentially personal information "
     "about living people. Anyone may copy it after publication.\n\n"
     "Repository: we6jbo/AncesTaleextended (master)\n"
     "File to publish: ghtree.json ONLY\n\n"
     "Inspect ~/Projects/AncesTaleextended/ghtree.json for private details "
     "before clicking Yes. Publishing cannot reliably be undone.";
 if(QMessageBox::warning(this,"Review family-tree publication",message,
                         QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
 if(!confirmAuthorization("publish the reviewed family tree ghtree.json to public GitHub"))return;
 callBridge({"publish-tree"},versionReport,{});
});
// Offer a private startup commit only after UI is visible. A canceled confirmation never commits.
QTimer::singleShot(0,this,[=]{
 if(QMessageBox::question(this,"Private startup version", "Your date and family-tree export was written locally without a Git commit. Request authorizej03 and create a private version now?", QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes){
    if(confirmAuthorization("commit the startup export"))callBridge({"commit-startup"},versionReport,{});
 }
});

auto *music=new QWidget;auto *ml=new QVBoxLayout(music);auto *url=new QLineEdit;url->setPlaceholderText("https://commons.wikimedia.org/wiki/File:...");ml->addWidget(url);auto *preview=new QPushButton("Read Commons license and metadata");auto *download=new QPushButton("Download reviewed recording locally");download->setEnabled(false);ml->addWidget(preview);ml->addWidget(download);auto *output=new QPlainTextEdit;output->setReadOnly(true);ml->addWidget(output);tabs->addTab(music,"Music");
auto *proc=new QProcess(this);auto *reviewed=new QString;connect(state,&QObject::destroyed,[reviewed]{delete reviewed;});
connect(url,&QLineEdit::textChanged,this,[=]{reviewed->clear();download->setEnabled(false);});
auto runMusic=[=](QString mode){if(proc->state()!=QProcess::NotRunning)return;preview->setEnabled(false);download->setEnabled(false);url->setEnabled(false);output->clear();proc->setProperty("mode",mode);proc->start("python3",{QCoreApplication::applicationDirPath()+"/music_import.py",mode,url->text().trimmed(),root});};
connect(preview,&QPushButton::clicked,this,[=]{runMusic("preview");});
connect(download,&QPushButton::clicked,this,[=]{if(*reviewed!=url->text().trimmed())return;if(QMessageBox::question(this,"Rights review","Have you checked the recording, composition, attribution and license terms? Save locally only?")==QMessageBox::Yes)runMusic("download");});
connect(proc,&QProcess::finished,this,[=](int exitCode,QProcess::ExitStatus exitStatus){output->setPlainText(QString::fromUtf8(proc->readAllStandardOutput()+proc->readAllStandardError()));preview->setEnabled(true);url->setEnabled(true);if(exitCode==0&&exitStatus==QProcess::NormalExit&&proc->property("mode").toString()=="preview"){*reviewed=url->text().trimmed();download->setEnabled(true);}else{reviewed->clear();download->setEnabled(false);}});
connect(proc,&QProcess::errorOccurred,this,[=](QProcess::ProcessError){output->setPlainText(proc->errorString());preview->setEnabled(true);url->setEnabled(true);});
connect(shot,&QPushButton::clicked,this,[=]{QString dir=QDir::homePath()+"/.38bTqEvQc/ancestaleextended";if(!QDir().mkpath(dir)){status->setText("Screenshot directory unavailable");return;}QString stamp=QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz");QString path=dir+"/AncesTaleextended-0.1.7-"+stamp+"-"+uid()+".png";auto old=heading->text();heading->setText(old+" | "+QDate::currentDate().toString(Qt::ISODate));captureFooter->show();QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);QPixmap image=grab();captureFooter->hide();heading->setText(old);if(!image.save(path,"PNG")){status->setText("Screenshot save failed");log("Screenshot save failed");return;}QFile::setPermissions(path,QFileDevice::ReadOwner|QFileDevice::WriteOwner);log("Screenshot saved "+path);status->setText("Screenshot saved locally; registering for review.");auto *reg=new QProcess(this);connect(reg,&QProcess::finished,this,[=](int c,QProcess::ExitStatus e){bool ok=c==0&&e==QProcess::NormalExit;status->setText(ok?"Screenshot registered; review status must be verified.":"Screenshot retained; registration failed.");log(ok?"Screenshot registration returned success":"Screenshot registration failed");reg->deleteLater();});connect(reg,&QProcess::errorOccurred,this,[=](QProcess::ProcessError){status->setText("Screenshot retained; infoartrecorder unavailable or failed.");log("Screenshot registration process error");reg->deleteLater();});if(QStandardPaths::findExecutable("infoartrecorder").isEmpty()){
 status->setText("Screenshot saved; optional infoartrecorder not installed."); reg->deleteLater();
}else reg->start("infoartrecorder",{"add",path,"--project","AncesTaleextended"});});
}
MainWindow::~MainWindow(){delete ui;}

void MainWindow::closeEvent(QCloseEvent *event) {
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

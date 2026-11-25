#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QMessageBox>
#include <QDateTime>
#include <QStringList>
#include <QStandardPaths>
#include <QRegularExpression>

static QString home=QDir::homePath();
static QString projects=home+"/Projects";
static bool runInTerminal(const QString &cmd){
 const auto kgx=QStandardPaths::findExecutable("kgx");
 if(kgx.isEmpty())return false;
 const QString workdir=home+"/Projects/AncesTaleextended";
 return QProcess::startDetached(kgx,{"--working-directory="+workdir,"--","bash","-lc",cmd},workdir);
}
static void appendOption(const QString &kind,const QString &selection,const QString &notes){
 QDir().mkpath(projects);
 QFile f(projects+"/AncesTaleextended/user-options.md");if(f.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)){
  f.write(("\n## "+QDateTime::currentDateTime().toString(Qt::ISODate)+"\nMode: "+kind+"\nSelection: "+selection+"\nUser instructions: "+notes+"\n").toUtf8());f.close();
 }
}
int main(int argc,char**argv){
 QApplication app(argc,argv);QWidget window;window.setWindowTitle("2menus — Vibe or installed local AI");window.resize(650,480);
 auto *v=new QVBoxLayout(&window);auto *label=new QLabel("Select an AI. Vibe is an agent; local Ollama chat models do not automatically gain shell, Git, or telnet permissions.");label->setWordWrap(true);v->addWidget(label);
 auto *choices=new QComboBox;choices->addItem("Mistral Vibe (agent)","vibe");
 QProcess proc;proc.start("ollama",{"list"});if(proc.waitForFinished(3000) && proc.exitCode()==0){
  const auto rows=QString::fromUtf8(proc.readAllStandardOutput()).split('\n');
  for(int i=1;i<rows.size();i++){auto s=rows[i].trimmed().split(QRegularExpression("\\s+"));if(!s.isEmpty()&&!s.first().isEmpty())choices->addItem("Ollama: "+s.first(),s.first());}
 }
 bool hasQwen=false;for(int i=0;i<choices->count();i++)if(choices->itemData(i).toString()=="qwen2.5:3b")hasQwen=true;
 if(!hasQwen)choices->addItem("Ollama: qwen2.5:3b (local)","qwen2.5:3b");
 v->addWidget(choices);
 auto *detail=new QPlainTextEdit;detail->setPlaceholderText("Optional instructions. Saved to ~/Projects/AncesTaleextended/user-options.md and read first by Vibe.");v->addWidget(detail);
 auto *start=new QPushButton("Start selected AI");auto *back=new QPushButton("Run genealogy program directly (no AI)");
 auto *runner=new QPushButton("Run reviewed daily macros (runner)");v->addWidget(start);v->addWidget(back);v->addWidget(runner);
 QObject::connect(start,&QPushButton::clicked,[&](){
  const QString selected=choices->currentData().toString();
  appendOption(selected=="vibe"?"Agent":"Local model",selected,detail->toPlainText());
  bool ok=false;
  if(selected=="vibe")ok=runInTerminal("exec bash \"$HOME/Projects/AncesTaleextended/.ao-ai-startup/aorun.sh\"");
  else {
   if(!QRegularExpression("^[A-Za-z0-9._:/-]+$").match(selected).hasMatch()){
    QMessageBox::warning(&window,"Invalid model ID","Ollama returned a model name that cannot safely be launched.");return;
   }
   // A local model operates as a chat session only. Do not misrepresent this as an autonomous agent.
   ok=runInTerminal("systemctl --user start qwen-local.service 2>/dev/null; sleep 2; cd \"$HOME/Projects/AncesTaleextended\"; OLLAMA_HOST=127.0.0.1:11434 ollama run '"+selected+"'");
  }
  if(!ok)QMessageBox::warning(&window,"Terminal unavailable","GNOME Console (kgx) could not start. Check that kgx is installed.");
 });
 QObject::connect(back,&QPushButton::clicked,[&](){const auto binary=home+"/.buildsq/AncesTaleextended/AncesTaleextended-program";
  if(!QProcess::startDetached(binary,{},projects+"/AncesTaleextended"))QMessageBox::critical(&window,"Launch failure","Cannot run "+binary);
 });
 QObject::connect(runner,&QPushButton::clicked,[&](){if(!QProcess::startDetached(home+"/.local/bin/runner",{},projects+"/AncesTaleextended/runner"))QMessageBox::warning(&window,"Runner unavailable","The runner has not been built or installed.");});
 window.show();return app.exec();
}

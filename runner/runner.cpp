#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QTimer>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QMessageBox>
#include <QComboBox>

int main(int argc,char**argv){
 QApplication app(argc,argv);QWidget window;window.setWindowTitle("runner — reviewed recurring tasks");window.resize(600,390);
 const auto home=QDir::homePath(),root=home+"/Projects/AncesTaleextended",report=home+"/Projects/runner-report.md";
 auto *l=new QVBoxLayout(&window);auto *info=new QLabel("Read-only startup macros. Automatically run after 10 seconds unless you change the selection or cancel. Source changes and GitHub publication are never automatic.");info->setWordWrap(true);l->addWidget(info);
 auto *choice=new QComboBox;
 choice->addItem("Project Git status (read-only)","git");
 choice->addItem("Check current SHA256 tree (read-only)","hash");
 choice->addItem("Check project build artifacts (read-only)","build");
 l->addWidget(choice);
 auto *counter=new QLabel("Auto-run in 10 seconds");l->addWidget(counter);
 auto *run=new QPushButton("Run selected task now");auto *cancel=new QPushButton("Stop countdown");l->addWidget(run);l->addWidget(cancel);
 auto *output=new QLabel;output->setWordWrap(true);output->setTextInteractionFlags(Qt::TextSelectableByMouse);l->addWidget(output);l->addStretch();
 int remaining=10;QTimer timer;timer.setInterval(1000);
 auto perform=[&](){timer.stop();QString command;QStringList args;
 const QString type=choice->currentData().toString();
 if(type=="git"){command="git";args={"-C",root,"status","--short","--branch"};}
 else if(type=="hash"){command="sha256sum";args={root+"/CMakeLists.txt",root+"/main.cpp",root+"/mainwindow.cpp"};}
 else {command="ls";args={"-lh",home+"/.buildsq/AncesTaleextended/AncesTaleextended-program"};}
 QProcess p;p.start(command,args);bool done=p.waitForFinished(10000);
 QString result=done?QString::fromUtf8(p.readAllStandardOutput()+p.readAllStandardError()):"TIMEOUT";
 if(!done){p.kill();p.waitForFinished(1000);}
 QString verdict=done&&p.exitCode()==0?"PASS":"FAIL";
 output->setText(verdict+" — "+result.left(1000));
 QFile f(report);if(f.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)){
  QTextStream out(&f);out<<"\n## "<<QDateTime::currentDateTime().toString(Qt::ISODate)<<"\nTask: "<<choice->currentText()<<"\nResult: "<<verdict<<"\n```\n"<<result.left(20000)<<"\n```\n";
 }
 remaining=10;counter->setText("Task complete. Select another task and Run now.");
 };
 QObject::connect(&timer,&QTimer::timeout,[&](){if(--remaining<=0)perform();else counter->setText(QString("Auto-run in %1 seconds").arg(remaining));});
 QObject::connect(run,&QPushButton::clicked,perform);
 QObject::connect(cancel,&QPushButton::clicked,[&](){timer.stop();counter->setText("Countdown stopped. Click Run now to execute.");});
 QObject::connect(choice,QOverload<int>::of(&QComboBox::currentIndexChanged),[&](int){remaining=10;if(!timer.isActive())timer.start();counter->setText("Auto-run in 10 seconds");});
 timer.start();window.show();return app.exec();
}

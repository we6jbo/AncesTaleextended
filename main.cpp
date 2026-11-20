#include <QApplication>
#include <QProcess>
#include <QCoreApplication>
#include <QMessageBox>
#include "mainwindow.h"
int main(int argc,char **argv) {
  QApplication app(argc,argv);
  app.setApplicationName("AncesTaleextended");
  app.setApplicationVersion("0.1.7");
  const QString helper=QCoreApplication::applicationDirPath()+"/tree_sync.py";
  QProcess startup;
  startup.start("python3",{helper,"startup"});
  const bool finished=startup.waitForFinished(30000);
  const QString message=QString::fromUtf8(startup.readAllStandardOutput()+startup.readAllStandardError());
  if (!finished || startup.exitStatus()!=QProcess::NormalExit || startup.exitCode()!=0)
    QMessageBox::warning(nullptr,"AncesTaleextended startup export",
      "Private date/tree export could not be completed. Check your source files.\n"+message.left(2000));
  MainWindow w; w.show(); return app.exec();
}

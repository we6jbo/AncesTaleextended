#include "hoeu_shutdown.h"
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>
#include <QApplication>
#include <QProcess>
#include <QCoreApplication>
#include <QMessageBox>
#include "mainwindow.h"
int main(int argc,char **argv) {
  QApplication app(argc,argv);
  // ANCESTALEEXTENDED_RUNNING_HOEU_SAFE_CLOSE_V2
  HoeuShutdown runningHoeuShutdown(&app);
  if (!runningHoeuShutdown.start()) return 1;

  // Replaced by the safe-close controller.


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

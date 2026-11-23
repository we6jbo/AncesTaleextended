#pragma once
#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimeZone>
#include <QProcess>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QClipboard>
#include <memory>

namespace ATENov27 {
inline void install(QWidget *owner, QTabWidget *tabs) {
    auto *page = new QWidget(tabs);
    auto *outer = new QVBoxLayout(page);
    auto *title = new QLabel("Family Tree Milestone Speech", page);
    QFont titleFont=title->font(); titleFont.setPointSize(titleFont.pointSize()+3); titleFont.setBold(true); title->setFont(titleFont);
    outer->addWidget(title);
    auto *deadline = new QLabel("Countdown to November 25, 2026 at 7:00 AM Pacific",page);
    outer->addWidget(deadline);
    auto *clock = new QLabel(page); clock->setAlignment(Qt::AlignCenter);
    QFont clockFont=clock->font(); clockFont.setPointSize(clockFont.pointSize()+9); clockFont.setBold(true); clock->setFont(clockFont);
    outer->addWidget(clock);
    auto updateClock = [clock]{
        const QTimeZone pacific("America/Los_Angeles");
        const QDateTime target(QDate(2026,11,25), QTime(7,0), pacific);
        qint64 secs=QDateTime::currentDateTimeUtc().secsTo(target);
        if(secs<=0) {clock->setText("00d 00h 00m 00s  |  Countdown complete");return;}
        const qint64 days=secs/86400; secs%=86400;
        const qint64 hours=secs/3600;secs%=3600;
        const qint64 mins=secs/60;secs%=60;
        clock->setText(QString("%1d %2h %3m %4s").arg(days,2,10,QChar('0')).arg(hours,2,10,QChar('0')).arg(mins,2,10,QChar('0')).arg(secs,2,10,QChar('0')));
    };
    updateClock();
    auto *clockTimer=new QTimer(page);clockTimer->setInterval(1000);
    QObject::connect(clockTimer,&QTimer::timeout,page,updateClock);clockTimer->start();

    auto *fact=new QLabel("Reading the current documented family relationships...",page);
    fact->setWordWrap(true);outer->addWidget(fact);
    auto *speech=new QPlainTextEdit(page);speech->setReadOnly(true);
    speech->setPlaceholderText("Verified figures will appear here once the quest evaluator responds.");
    outer->addWidget(speech,1);
    auto *buttons=new QHBoxLayout;
    auto *refresh=new QPushButton("Refresh actual genealogy figures",page);
    auto *copy=new QPushButton("Copy speech",page);
    buttons->addWidget(refresh);buttons->addWidget(copy);outer->addLayout(buttons);
    auto *status=new QLabel("Uses the same verified quest evaluator as Family Connection Quest. No research data is modified.",page);
    status->setWordWrap(true);outer->addWidget(status);
    tabs->addTab(page,"11-27-2026");
    auto running=std::make_shared<bool>(false);
    auto update=[=]{
        if(*running)return;
        *running=true;refresh->setEnabled(false);status->setText("Refreshing documented quest data...");
        auto *proc=new QProcess(page);
        proc->setProgram("python3");
        proc->setArguments({QStringLiteral("/home/we6jbo/Projects/AncesTaleextended/mission_bridge.py"),"quests"});
        QObject::connect(proc,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),page,[=](int code,QProcess::ExitStatus exit){
            const QByteArray output=proc->readAllStandardOutput();
            const QByteArray errors=proc->readAllStandardError();
            *running=false;refresh->setEnabled(true);proc->deleteLater();
            QJsonParseError parseError;
            const QJsonDocument doc=QJsonDocument::fromJson(output,&parseError);
            if(code!=0 || exit!=QProcess::NormalExit || parseError.error!=QJsonParseError::NoError || !doc.isObject() || doc.object().contains("error")){
                status->setText("Quest data unavailable. The existing speech has not been overwritten. "+QString::fromUtf8(errors.left(200)));
                return;
            }
            const auto obj=doc.object();
            if(!obj.value("goals").isArray() || !obj.value("verified_edges").isDouble() || !obj.value("percentage").isDouble()){
                status->setText("Quest response missing required fields; existing speech retained.");return;
            }
            const int completed=obj.value("completed").toInt();
            const int total=obj.value("total").toInt();
            const int verified=obj.value("verified_edges").toInt();
            const QString percent=QString::number(obj.value("percentage").toDouble(),'f',2);
            QSet<QString> uniquePeople;
            for(const auto goal:obj.value("goals").toArray()){
                const QJsonObject g=goal.toObject();
                if(!g.value("completed").toBool())continue;
                for(const auto v:g.value("path").toArray()){
                    const QString id=v.toObject().value("id").toString();
                    if(!id.isEmpty())uniquePeople.insert(id);
                }
            }
            const int people=uniquePeople.size();
            const QString statement=QString(
                "Good evening! I'm at the top of Cowles Mountain. I've successfully published AncesTaleextended to GitHub. "
                "The program has identified %1 people along verified family-connection paths and documented %2 verified relationships. "
                "Progress toward the next family tree milestone is currently %3 percent. "
                "Here's a fun fact: we're tracking %4 family connection quests, and %5 are complete. "
                "It also integrates with a website where visitors can suggest additional family records for review. "
                "To learn more, visit my website at jay zero three dot page. Thanks, and have a great night!"
            ).arg(people).arg(verified).arg(percent).arg(total).arg(completed);
            speech->setPlainText(statement);
            fact->setText(QString("Verified people on completed paths: %1  |  Verified relationships: %2  |  Completed goals: %3/%4  |  Progress: %5%")
                          .arg(people).arg(verified).arg(completed).arg(total).arg(percent));
            status->setText("Updated from recorded family links. Zero means no qualifying verified links yet, not that no family members exist.");
        });
        QObject::connect(proc,&QProcess::errorOccurred,page,[=](QProcess::ProcessError){
            if(!*running)return;
            *running=false;refresh->setEnabled(true);
            status->setText("Could not start the quest evaluator: "+proc->errorString());proc->deleteLater();
        });
        proc->start();
    };
    QObject::connect(refresh,&QPushButton::clicked,page,update);
    QObject::connect(copy,&QPushButton::clicked,page,[=]{
        if(!speech->toPlainText().isEmpty())QApplication::clipboard()->setText(speech->toPlainText());
    });
    auto *updateTimer=new QTimer(page);updateTimer->setInterval(30000);
    QObject::connect(updateTimer,&QTimer::timeout,page,update);updateTimer->start();
    QTimer::singleShot(0,page,update);
}
}

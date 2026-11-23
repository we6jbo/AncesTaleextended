#pragma once
#include "classical_ai.h"
#include <QPointer>
#include <functional>
#include <memory>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>
#include <QTcpSocket>
#include <QRegularExpression>

namespace ATEClassicalNet {
inline QStringList names(){return ATEClassical::applications();}
inline QStringList keys(){return {"moltbook_skeleton","carventory_commands","textwarden_tested","edutrace_examined"};}
inline QString exeName(int i){return (QStringList{"","carventory","textwarden","edutrace"}).value(i);}
inline bool harmless(const QStringList &a){
    if(a.isEmpty())return false;
    const QString first=a.first().toLower();
    return QStringList{"help","--help","-h","--version","version","status","list","sources"}.contains(first);
}
inline bool saveSettings(const QJsonObject &settings){return ATEClassical::store("the-setup.json",QJsonDocument(settings).toJson(QJsonDocument::Indented));}
inline bool saveState(const QJsonObject &state){return ATEClassical::store("start-me.json",QJsonDocument(state).toJson(QJsonDocument::Indented));}
inline void record(const QString &kind,int i,const QString &detail){
    const QString app=names().value(i);
    ATEClassical::append("exploration.jsonl",{{"utc",ATEClassical::now()},{"kind",kind},{"application",app},{"detail",detail.left(8000)}});
    QFile f(ATEClassical::outputDir()+"/exploration.md");
    if(f.open(QIODevice::Append|QIODevice::Text)){
        QString line=detail.left(600);line.replace('\n',' ');line.replace('\r',' ');
        f.write(("\n- "+ATEClassical::now()+" | "+app+" | "+kind+" | "+line+"\n").toUtf8());
        f.close();QFile::setPermissions(f.fileName(),QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    }
}
inline QString resolvedExe(int i,const QJsonObject &config){
    QString s=config.value("executable").toString(exeName(i));
    if(i==0)return {};
    if(QFileInfo(s).isAbsolute())return QFileInfo(s).isExecutable()&&QFileInfo(s).isFile()?s:QString();
    return QStandardPaths::findExecutable(s);
}
inline void help(const std::function<void(const QString&)> &send,int selected){
    send("AI CONSULTATION (network only, no Classical/AI GUI tabs)");
    for(int i=0;i<names().size();++i)send(QString("AI USE %1 - %2").arg(i+1).arg(names()[i]));
    send("AI STATUS | AI USE <1-4> | AI PROBE | AI RUN <read-only args> | AI NOTE <text> | AI DRAFT <text> | AI REVIEW <review notes> | AI RATE <1-10> | AI HELP");
    send(QString("Selected: %1").arg(names().value(selected)));
    send("AI RUN allows read-only help/version/status/list/sources actions; no shell evaluation or automatic posting.");
}
inline bool handle(QString line,int &selected,QPointer<QTcpSocket> socket,const std::function<void(const QString&)> &send){
    if(line.compare("AI",Qt::CaseInsensitive)==0 || line.compare("AI HELP",Qt::CaseInsensitive)==0){help(send,selected);return true;}
    if(!line.startsWith("AI ",Qt::CaseInsensitive))return false;
    const QString command=line.mid(3).trimmed();
    if(command.compare("STATUS",Qt::CaseInsensitive)==0){
        const auto state=ATEClassical::readObject("start-me.json");
        send(QString("Classical setup: %1").arg(ATEClassical::ready(state)?"complete":"incomplete"));
        for(int i=0;i<4;++i)send(QString("%1. %2: %3").arg(i+1).arg(names()[i],state.value(keys()[i]).toBool()?"reviewed":"pending"));
        send("Selected: "+names().value(selected));return true;
    }
    if(command.startsWith("USE ",Qt::CaseInsensitive)){
        bool ok=false;int n=command.mid(4).trimmed().toInt(&ok);
        if(!ok||n<1||n>4){send("Choose AI USE 1, 2, 3, or 4");return true;}
        selected=n-1;send("Selected: "+names()[selected]);
        send("AI PROBE | AI RUN <read-only args> | AI NOTE <text> | AI DRAFT <text> | AI REVIEW <notes> | AI RATE <1-10>");return true;
    }
    if(selected<0||selected>=4){send("Use AI USE 1-4 first");return true;}
    if(command.startsWith("NOTE ",Qt::CaseInsensitive)||command.startsWith("DRAFT ",Qt::CaseInsensitive)){
        const bool draft=command.startsWith("DRAFT ",Qt::CaseInsensitive);
        QString value=command.mid(draft?6:5).trimmed();
        if(value.isEmpty()||value.size()>6000){send("Enter 1-6000 characters");return true;}
        QString file=draft?"consultation.jsonl":"exploration.jsonl";
        bool ok=ATEClassical::append(file,{{"utc",ATEClassical::now()},{"app",names()[selected]},{"kind",draft?"local-draft":"note"},{"text",value}});
        send(ok?"Saved locally; nothing published":"ERROR saving record");return true;
    }
    if(command.startsWith("RATE ",Qt::CaseInsensitive)){
        bool valid=false;int rating=command.mid(5).trimmed().toInt(&valid);
        if(!valid||rating<1||rating>10){send("Use AI RATE 1 through AI RATE 10");return true;}
        bool ok=ATEClassical::append("feedback.jsonl",{{"utc",ATEClassical::now()},{"app",names()[selected]},{"rating",rating}});
        send(ok?"Rating saved":"ERROR saving rating");return true;
    }
    if(command.startsWith("REVIEW ",Qt::CaseInsensitive)){
        QString notes=command.mid(7).trimmed();
        if(notes.isEmpty()||notes.size()>4000){send("Provide review notes, 1-4000 characters");return true;}
        auto settings=ATEClassical::readObject("the-setup.json");
        auto state=ATEClassical::readObject("start-me.json");
        QJsonObject item=settings.value(names()[selected]).toObject();
        item.insert("notes",notes);item.insert("reviewed_utc",ATEClassical::now());
        if(selected==0){
            QJsonObject skeleton{{"kind","moltbook-integration-skeleton"},{"status","local-only"},{"publishing_enabled",false},{"created_utc",ATEClassical::now()}};
            if(!ATEClassical::store("moltbook-skeleton.json",QJsonDocument(skeleton).toJson(QJsonDocument::Indented))){send("ERROR saving Moltbook skeleton");return true;}
        }
        settings.insert(names()[selected],item);
        state.insert(keys()[selected],true);
        state.insert("last_updated_utc",ATEClassical::now());
        if(!saveSettings(settings)||!saveState(state)){send("ERROR saving setup status");return true;}
        record("review-complete",selected,notes);
        send("Review recorded. This acknowledges manual examination, not verified full integration.");
        if(ATEClassical::ready(state))send("All four setup reviews completed. AI consultation commands are now available on port 53141.");
        return true;
    }
    if(command.compare("PROBE",Qt::CaseInsensitive)==0||command.startsWith("RUN ",Qt::CaseInsensitive)){
        if(selected==0){send("Moltbook is an online forum. No local executable is configured; AI DRAFT writes a local draft only.");return true;}
        const auto settings=ATEClassical::readObject("the-setup.json");
        const auto config=settings.value(names()[selected]).toObject();
        QString exe=resolvedExe(selected,config);
        if(exe.isEmpty()){send("Executable unavailable. Verify installation and PATH.");return true;}
        QStringList args;
        if(command.compare("PROBE",Qt::CaseInsensitive)==0)args=(selected==3)?QStringList{"help"}:QStringList{"--help"};
        else args=QProcess::splitCommand(command.mid(4).trimmed());
        if(!harmless(args)){send("Only read-only help, version, status, list, and sources commands are permitted by this bridge.");return true;}
        send("Running "+exe+" "+args.join(" "));
        auto *p=new QProcess(socket);p->setProcessChannelMode(QProcess::MergedChannels);
        auto *timer=new QTimer(p);timer->setSingleShot(true);
        QObject::connect(timer,&QTimer::timeout,p,[p]{if(p->state()!=QProcess::NotRunning)p->kill();});
        auto output=std::make_shared<QByteArray>();
        QObject::connect(p,&QProcess::readyRead,p,[p,output]{if(output->size()<32768)output->append(p->readAll().left(32768-output->size()));else p->readAll();});
        QObject::connect(p,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),p,[=](int code,QProcess::ExitStatus es){
            output->append(p->readAll().left(qMax(0,32768-output->size())));
            QString result=QString::fromUtf8(*output);
            record("command-result",selected,QString("exe=%1 args=%2 exit=%3 normal=%4 output=%5").arg(exe,args.join(" ")).arg(code).arg(es==QProcess::NormalExit).arg(result.left(5000)));
            ATEClassical::append("consultation.jsonl",{{"utc",ATEClassical::now()},{"app",names()[selected]},{"executable",exe},{"args",QJsonArray::fromStringList(args)},{"exit",code},{"output",result.left(16000)}});
            if(socket){socket->write((result+QString("\nExit=%1 normal=%2\n").arg(code).arg(es==QProcess::NormalExit)).toUtf8());}
            p->deleteLater();
        });
        QObject::connect(p,&QProcess::errorOccurred,p,[=](QProcess::ProcessError){
            if(socket)socket->write(("ERROR: "+p->errorString()+"\n").toUtf8());
        });
        p->start(exe,args);timer->start(10000);
        return true;
    }
    send("Unknown AI command. Enter AI HELP.");return true;
}
} // namespace ATEClassicalNet

#pragma once
#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QProcess>
#include <QStandardPaths>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>

// All persistent records for this module are under the requested project subdirectory.
namespace ATEClassical {
inline QString projectDir(){return QDir::homePath()+"/Projects/AncesTaleextended";}
inline QString outputDir(){return projectDir()+"/classical-ai-design";}
inline const QStringList applications(){return {"Moltbook","carventory","textwarden","edutrace"};}
inline QString slug(const QString &s){return s.toLower();}
inline bool store(const QString &name,const QByteArray &data){
    QDir().mkpath(outputDir());
    QSaveFile f(outputDir()+"/"+name);
    if(!f.open(QIODevice::WriteOnly))return false;
    if(f.write(data)!=data.size()||!f.commit())return false;
    QFile::setPermissions(outputDir()+"/"+name,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    return true;
}
inline QJsonObject readObject(const QString &name){
    QFile f(outputDir()+"/"+name);
    if(!f.open(QIODevice::ReadOnly))return {};
    auto d=QJsonDocument::fromJson(f.readAll());
    return d.isObject()?d.object():QJsonObject{};
}
inline bool append(const QString &name,const QJsonObject &o){
    QDir().mkpath(outputDir());
    QFile f(outputDir()+"/"+name);
    if(!f.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text))return false;
    QByteArray line=QJsonDocument(o).toJson(QJsonDocument::Compact)+"\n";
    bool ok=f.write(line)==line.size();f.close();
    QFile::setPermissions(outputDir()+"/"+name,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    return ok;
}
inline QString now(){return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);}
inline bool ready(const QJsonObject &state){
    for(const auto &key:{"moltbook_skeleton","carventory_commands","textwarden_tested","edutrace_examined"})
        if(!state.value(key).toBool(false))return false;
    return true;
}
inline QString keyFor(int n){return (QStringList{"moltbook_skeleton","carventory_commands","textwarden_tested","edutrace_examined"}).value(n);}
inline QString defaultExe(int n){return (QStringList{"","carventory","textwarden","edutrace"}).value(n);}
inline void install(QMainWindow *window,QTabWidget *tabs){
    QDir().mkpath(outputDir());
    QFile::setPermissions(outputDir(),QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
    auto *state=new QJsonObject(readObject("start-me.json"));
    auto *settings=new QJsonObject(readObject("the-setup.json"));
    auto *owned=new QObject(tabs);
    QObject::connect(owned,&QObject::destroyed,[state,settings]{delete state;delete settings;});
    auto saveState=[=](){
        state->insert("schema",1);state->insert("last_updated_utc",now());
        return store("start-me.json",QJsonDocument(*state).toJson(QJsonDocument::Indented));
    };
    auto saveSettings=[=](){settings->insert("last_updated_utc",now());return store("the-setup.json",QJsonDocument(*settings).toJson(QJsonDocument::Indented));};
    auto record=[=](const QString &kind,const QString &name,const QString &detail){
        append("exploration.jsonl",{{"utc",now()},{"kind",kind},{"application",name},{"detail",detail.left(8000)}});
        QFile f(outputDir()+"/exploration.md");
        if(f.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)){
            auto scrub=[](QString s){return s.replace("\r", " ").replace("\n"," ");};
            QByteArray b=("\n- "+now()+" | "+scrub(name)+" | "+scrub(kind)+" | "+scrub(detail.left(350))+"\n").toUtf8();f.write(b);f.close();
            QFile::setPermissions(f.fileName(),QFileDevice::ReadOwner|QFileDevice::WriteOwner);
        }
    };
    // Native external programs are probed only by invoking their declared executable directly.
    // Never a shell, eval, browser automation, or surprise publication.
    auto probe=[=](int n,const QString &exeName,QPlainTextEdit *out){
        QString exe=QStandardPaths::findExecutable(exeName.trimmed());
        if(exe.isEmpty() && QFileInfo(exeName).isExecutable() && QFileInfo(exeName).isFile())exe=exeName;
        if(exe.isEmpty()){out->appendPlainText("Executable unavailable: "+exeName);record("probe-unavailable",applications()[n],exeName);return;}
        QStringList helpArgs=(n==3)?QStringList{"help"}:QStringList{"--help"};
        out->appendPlainText("Testing: "+exe+" "+helpArgs.join(" "));
        auto *p=new QProcess(out);p->setProcessChannelMode(QProcess::MergedChannels);
        auto *timer=new QTimer(p);timer->setSingleShot(true);
        QObject::connect(timer,&QTimer::timeout,p,[=]{if(p->state()!=QProcess::NotRunning)p->kill();});
        QObject::connect(p,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),out,[=](int ec,QProcess::ExitStatus es){
            QString result=QString::fromUtf8(p->readAll()).left(18000);
            out->appendPlainText(result);
            out->appendPlainText(QString("Completed: exit=%1 normal=%2").arg(ec).arg(es==QProcess::NormalExit));
            record("probe-help",applications()[n],QString("exe=%1 exit=%2 normal=%3 output=%4").arg(exe).arg(ec).arg(es==QProcess::NormalExit).arg(result.left(6500)));
            p->deleteLater();
        });
        QObject::connect(p,&QProcess::errorOccurred,out,[=](QProcess::ProcessError){
            out->appendPlainText("Error: "+p->errorString());record("probe-error",applications()[n],p->errorString());
        });
        p->start(exe,helpArgs);timer->start(8000);
    };
    auto *setupPage=new QWidget(tabs);auto *setupLayout=new QVBoxLayout(setupPage);
    auto *info=new QLabel("Classical AI design: explore each integration, save your choices, then finish setup. Checks represent your review, not automatic proof of integration.");
    info->setWordWrap(true);setupLayout->addWidget(info);
    auto *checkBoxes=new QVector<QCheckBox*>;auto *exeBoxes=new QVector<QLineEdit*>;auto *notesBoxes=new QVector<QPlainTextEdit*>;
    QObject::connect(owned,&QObject::destroyed,[checkBoxes,exeBoxes,notesBoxes]{delete checkBoxes;delete exeBoxes;delete notesBoxes;});
    auto *setupViews=new QTabWidget(setupPage);setupViews->tabBar()->setUsesScrollButtons(true);setupViews->tabBar()->setExpanding(false);setupLayout->addWidget(setupViews,1);
    QStringList descriptions={
      "Create and inspect a local Moltbook integration skeleton. No Moltbook API call or post is made during setup.",
      "Discover the carventory executable and analyze its --help output. Carventory tracks items in the 2021 Subaru Outback.",
      "Find and probe the TextWarden executable. Record what you actually tested against Sufficiently Educated.",
      "Find and examine Edutrace commands and document how it analyzes special-education information."
    };
    QStringList labels={"Moltbook skeleton inspected","Carventory commands analyzed","TextWarden tested","Edutrace examined"};
    for(int i=0;i<4;i++){
        auto *page=new QWidget(setupViews);auto *v=new QVBoxLayout(page);
        auto *explain=new QLabel(descriptions[i]);explain->setWordWrap(true);v->addWidget(explain);
        auto *exe=new QLineEdit(settings->value(slug(applications()[i])).toObject().value("executable").toString(defaultExe(i)));
        exe->setPlaceholderText(i==0?"Optional installed Moltbook CLI executable":"Executable name or absolute path");
        exe->setObjectName("classical-"+slug(applications()[i])+"-executable");
        v->addWidget(new QLabel("Optional program executable:"));v->addWidget(exe);exeBoxes->append(exe);
        auto *actions=new QHBoxLayout;auto *probeButton=new QPushButton("Explore --help");
        auto *createButton=new QPushButton(i==0?"Create local Moltbook skeleton":"Record executable discovery");
        actions->addWidget(probeButton);actions->addWidget(createButton);v->addLayout(actions);
        auto *out=new QPlainTextEdit;out->setReadOnly(true);out->setPlaceholderText("Actual exploration results appear here");v->addWidget(out,1);
        auto *notes=new QPlainTextEdit;notes->setPlaceholderText("What commands, interfaces, or use cases did you examine? Record exact findings and limitations.");
        notes->setPlainText(settings->value(slug(applications()[i])).toObject().value("notes").toString());v->addWidget(notes);notesBoxes->append(notes);
        auto *check=new QCheckBox(labels[i]);check->setChecked(state->value(keyFor(i)).toBool());v->addWidget(check);checkBoxes->append(check);
        setupViews->addTab(page,applications()[i]);
        QObject::connect(probeButton,&QPushButton::clicked,page,[=]{probe(i,exe->text(),out);});
        QObject::connect(createButton,&QPushButton::clicked,page,[=]{
            if(i==0){
                QJsonObject skeleton{{"kind","moltbook-forum-integration"},{"status","local-skeleton-only"},{"publishing_enabled",false},{"approval_required",true},{"created_utc",now()},{"description","AI forum; API authentication and actual posting are not verified by this skeleton"}};
                bool ok=store("moltbook-skeleton.json",QJsonDocument(skeleton).toJson(QJsonDocument::Indented));
                out->appendPlainText(ok?"Local skeleton created. No network request or post made.":"Could not create local skeleton.");
                record("skeleton",applications()[i],ok?"Local skeleton created":"Failed to write skeleton");
            }else{
                auto resolved=QStandardPaths::findExecutable(exe->text());
                out->appendPlainText(resolved.isEmpty()?"Executable not found in PATH":"Found executable: "+resolved);
                record("discovery",applications()[i],resolved.isEmpty()?"unavailable":"found "+resolved);
            }
        });
        QObject::connect(check,&QCheckBox::toggled,page,[=](bool yes){
            state->insert(keyFor(i),yes);saveState();record("review-check",applications()[i],yes?"checked by user":"unchecked by user");
        });
    }
    auto *finish=new QPushButton("Save setup and open AI consultation");setupLayout->addWidget(finish);
    auto *consultPage=new QWidget(tabs);auto *consultLayout=new QVBoxLayout(consultPage);
    auto *consultInfo=new QLabel("Consultation uses real program executables where available. No actual Moltbook posting or third-party data upload occurs automatically.");
    consultInfo->setWordWrap(true);consultLayout->addWidget(consultInfo);
    auto *appTabs=new QTabWidget(consultPage);appTabs->tabBar()->setUsesScrollButtons(true);appTabs->tabBar()->setExpanding(false);consultLayout->addWidget(appTabs,1);
    for(int i=0;i<4;i++){
        auto *page=new QWidget(appTabs);auto *v=new QVBoxLayout(page);
        auto *mode=new QComboBox;mode->addItems({"Review help","Run command with arguments","Save a local draft or question"});
        mode->setObjectName("consult-"+slug(applications()[i])+"-mode");v->addWidget(mode);
        auto *exe=new QLineEdit(settings->value(slug(applications()[i])).toObject().value("executable").toString(defaultExe(i)));
        exe->setPlaceholderText("Program executable (no shell)");exe->setObjectName("consult-"+slug(applications()[i])+"-executable");v->addWidget(exe);
        auto *arguments=new QLineEdit;arguments->setPlaceholderText("Arguments (e.g. --version). No shell operators are evaluated.");arguments->setObjectName("consult-"+slug(applications()[i])+"-args");v->addWidget(arguments);
        auto *question=new QPlainTextEdit;question->setPlaceholderText("Question, draft, or consultation notes. Saved locally when requested.");v->addWidget(question,1);
        auto *row=new QHBoxLayout;auto *run=new QPushButton("Run selected action");auto *openFolder=new QPushButton("Open local output folder");row->addWidget(run);row->addWidget(openFolder);v->addLayout(row);
        auto *result=new QPlainTextEdit;result->setReadOnly(true);result->setPlaceholderText("Observed result, errors, and exit status");v->addWidget(result,1);
        auto *rateLabel=new QLabel("How useful was this result? 1 = difficult, 10 = got what I wanted");v->addWidget(rateLabel);
        auto *rating=new QSpinBox;rating->setRange(1,10);rating->setValue(5);rating->setObjectName("consult-"+slug(applications()[i])+"-rating");v->addWidget(rating);
        auto *feedback=new QPlainTextEdit;feedback->setPlaceholderText("Optional rating explanation");feedback->setMaximumHeight(75);v->addWidget(feedback);
        auto *saveRating=new QPushButton("Save 1–10 feedback");v->addWidget(saveRating);
        appTabs->addTab(page,applications()[i]);
        QObject::connect(openFolder,&QPushButton::clicked,page,[=]{QDesktopServices::openUrl(QUrl::fromLocalFile(outputDir()));});
        QObject::connect(run,&QPushButton::clicked,page,[=]{
            QString app=applications()[i];
            if(mode->currentIndex()==2){
                bool ok=append("consultation.jsonl",{{"utc",now()},{"app",app},{"kind","draft"},{"text",question->toPlainText()}});
                result->appendPlainText(ok?"Draft saved locally. Nothing published or sent externally.":"Unable to save draft.");
                record("draft",app,ok?"draft saved locally":"draft write failed");return;
            }
            QString cmd=exe->text().trimmed();if(cmd.isEmpty()){result->appendPlainText("Enter a program executable first.");return;}
            QString resolved=QStandardPaths::findExecutable(cmd);
            if(resolved.isEmpty() && QFileInfo(cmd).isFile() && QFileInfo(cmd).isExecutable())resolved=cmd;
            if(resolved.isEmpty()){result->appendPlainText("Program not found: "+cmd);return;}
            QStringList args=mode->currentIndex()==0?(i==3?QStringList{"help"}:QStringList{"--help"}):QProcess::splitCommand(arguments->text());
            result->appendPlainText("Invoking "+resolved+" "+args.join(' '));
            auto *p=new QProcess(page);p->setProcessChannelMode(QProcess::MergedChannels);
            auto *timeout=new QTimer(p);timeout->setSingleShot(true);
            QObject::connect(timeout,&QTimer::timeout,p,[=]{if(p->state()!=QProcess::NotRunning)p->kill();});
            QObject::connect(p,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),page,[=](int exitCode,QProcess::ExitStatus exitStatus){
                QString observed=QString::fromUtf8(p->readAll()).left(16000);
                result->appendPlainText(observed);
                result->appendPlainText(QString("Exit=%1 normal=%2").arg(exitCode).arg(exitStatus==QProcess::NormalExit));
                append("consultation.jsonl",{{"utc",now()},{"app",app},{"kind","execution"},{"executable",resolved},{"arguments",QJsonArray::fromStringList(args)},{"exit",exitCode},{"normal",exitStatus==QProcess::NormalExit},{"output",observed}});
                p->deleteLater();
            });
            QObject::connect(p,&QProcess::errorOccurred,page,[=](QProcess::ProcessError){result->appendPlainText("Process error: "+p->errorString());});
            p->start(resolved,args);timeout->start(15000);
        });
        QObject::connect(saveRating,&QPushButton::clicked,page,[=]{
            bool ok=append("feedback.jsonl",{{"utc",now()},{"app",applications()[i]},{"rating",rating->value()},{"comment",feedback->toPlainText()},{"last_visible_output",result->toPlainText().right(1500)}});
            result->appendPlainText(ok?"Feedback saved.":"Feedback write failed.");
        });
    }
    auto activate=[=](){
        int old=tabs->indexOf(setupPage);if(old>=0)tabs->removeTab(old);
        if(tabs->indexOf(consultPage)<0)tabs->addTab(consultPage,"AI consultation");
        tabs->setCurrentWidget(consultPage);
    };
    QObject::connect(finish,&QPushButton::clicked,setupPage,[=]{
        for(int i=0;i<4;i++){
            QJsonObject row{{"executable",exeBoxes->at(i)->text().trimmed()},{"notes",notesBoxes->at(i)->toPlainText()},{"reviewed",checkBoxes->at(i)->isChecked()}};
            settings->insert(slug(applications()[i]),row);
        }
        if(!saveSettings()||!saveState()){info->setText("ERROR: setup could not be written. Retaining Classical tab.");return;}
        record("setup", "all", "setup saved by user");
        if(!ready(*state)){info->setText("Setup saved. Complete all four review checkmarks to open AI consultation.");return;}
        if(!QFileInfo(outputDir()+"/moltbook-skeleton.json").isFile()){
            info->setText("Create and inspect the local Moltbook skeleton before finishing.");return;
        }
        activate();
    });
    if(ready(*state)&&QFileInfo(outputDir()+"/moltbook-skeleton.json").isFile())activate();
    else {tabs->addTab(setupPage,"Classical");tabs->setCurrentWidget(setupPage);saveState();}
}
} // namespace ATEClassical

#pragma once
#include <QtWidgets>
#include <QStorageInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrlQuery>
#include <QElapsedTimer>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>
#include <functional>
#include <cmath>

namespace ATEPower {
inline QByteArray bytes(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
inline double availableGB(){
    const auto lines=bytes("/proc/meminfo").split('\n');
    for(const auto &line:lines)if(line.startsWith("MemAvailable:")){
        const auto bits=line.simplified().split(' ');
        if(bits.size()>=2)return bits[1].toDouble()/1000000.0;
    }
    return -1;
}
inline double diskGB(){
    QStorageInfo info(QDir::homePath()+"/Projects/AncesTaleextended");
    if(!info.isValid()||!info.isReady())return -1;
    return double(info.bytesAvailable())/1000000000.0;
}
inline double batteryW(){
    QDir d("/sys/class/power_supply");
    for(const QString &name:d.entryList(QDir::Dirs|QDir::NoDotAndDotDot)){
        const QString base=d.absoluteFilePath(name)+"/";
        if(QString::fromUtf8(bytes(base+"type")).trimmed().compare("Battery",Qt::CaseInsensitive)!=0)continue;
        const QString status=QString::fromUtf8(bytes(base+"status")).trimmed();
        if(status.compare("Discharging",Qt::CaseInsensitive)!=0)continue;
        bool ok=false;
        const double microwatts=QString::fromUtf8(bytes(base+"power_now")).trimmed().toDouble(&ok);
        if(ok)return microwatts/1000000.0;
        bool a=false,b=false;
        const double uA=QString::fromUtf8(bytes(base+"current_now")).trimmed().toDouble(&a);
        const double uV=QString::fromUtf8(bytes(base+"voltage_now")).trimmed().toDouble(&b);
        if(a&&b)return uA*uV/1e12;
    }
    return -1;
}
struct Cpu {quint64 idle=0,total=0;bool valid=false;};
inline Cpu cpuSample(){
    const auto data=bytes("/proc/stat");
    const auto first=data.split('\n').value(0).simplified().split(' ');
    if(first.size()<5||first[0]!="cpu")return {};
    Cpu s; s.valid=true;
    for(int i=1;i<first.size();++i){bool ok=false;quint64 n=first[i].toULongLong(&ok);if(!ok){s.valid=false;break;}s.total+=n;if(i==4||i==5)s.idle+=n;}
    return s;
}
inline double cpuPercent(const Cpu &a,const Cpu &b){
    if(!a.valid||!b.valid||b.total<=a.total)return -1;
    if(b.idle<a.idle)return -1;
    return 100.0*(1.0-double(b.idle-a.idle)/double(b.total-a.total));
}
inline QString f(double n,int p=2){return n<0?"unavailable":QString::number(n,'f',p);}
inline QString health(double cpu){
    const double bat=batteryW(),mem=availableGB(),disk=diskGB();
    QStringList warnings;
    if(bat>=15.2)warnings<<"battery discharge at or above 15.2 W";
    if(cpu>63.4)warnings<<"CPU above 63.4%";
    if(mem>=0&&mem<5.36)warnings<<"available RAM below 5.36 GB";
    if(disk>=0&&disk<100.0)warnings<<"free disk below 100 GB";
    QString output=QString("CPU: %1% (sampled) | Battery draw: %2 W (discharging only) | RAM available: %3 GB | Disk free: %4 GB")
        .arg(f(cpu,1),f(bat),f(mem),f(disk));
    output+="\n"+(warnings.isEmpty()?"System thresholds: none exceeded (unavailable measurements are not counted).":
      "COMPUTER TAXED: Evaluate system performance. Reasons: "+warnings.join("; "));
    return output;
}
inline QStringList models(){return {"Llama 3.2", "Qwen3", "LLaMA (other)", "Phi-4", "Qwen (other)"};}
inline QString modelReport(const QString &listing){
    QStringList found;
    const auto lines=listing.split('\n',Qt::SkipEmptyParts);
    for(const auto &line:lines){const auto bits=line.simplified().split(' ');if(bits.isEmpty())continue;found<<bits.first().toLower();}
    auto has=[&](const QString &prefix){for(const auto &s:found)if(s.startsWith(prefix))return true;return false;};
    return QString("Llama 3.2: %1\nQwen3: %2\nLLaMA family: %3\nPhi-4: %4\nQwen family: %5")
      .arg(has("llama3.2")?"installed":"not listed",has("qwen3")?"installed":"not listed",
           (has("llama")||has("codellama"))?"installed":"not listed",has("phi4")?"installed":"not listed",
           has("qwen")?"installed":"not listed");
}
inline QString guide(){return QString::fromUtf8(R"TXT(POWER UP | Local AI fallback (advisory; no automatic downloads)
Check Ollama is installed: command -v ollama
Check models: ollama list
Official Ollama install instructions: https://ollama.com/download/linux
Examples (download only after checking storage, battery, RAM and licenses):
  ollama pull llama3.2:1b
  ollama pull qwen3:0.6b
  ollama pull phi4
  ollama pull qwen2.5:0.5b
  ollama pull llama3.1:8b
The labels "Lama" and "Qwen" are model families/ambiguous names, not unique model identifiers.
Larger models such as Phi-4 and LLaMA 8B may be unsuitable for current hardware.
Mistral Vibe wiring plan: use QProcess for `ollama list`, or a QNetworkAccessManager request to the local Ollama API (127.0.0.1:11434/api/chat). Send user-approved prompts only, parse the JSON response, use timeouts and cancellation, and log model, latency, memory impact and errors. Never place credentials or private genealogy data in external search URLs. Ask Vibe to create and review the integration before enabling any automatic fallback.
Fallback rule: when Vibe reports an action is unavailable, ask before sending that task to a selected local model. A local model only returns suggestions; it must not autonomously execute privileged shell commands, publish records or overwrite family data.
Publishing workflow idea: 4-word blog title, exactly 2 substantive paragraphs with accurate search-friendly language, Facebook and LinkedIn excerpts linking to the WordPress post, and a short spoken script for Cowles Mountain (about 40 seconds by default, well under a 40-minute maximum). Review all public content before publishing.
Search engines: DuckDuckGo, Google, Yahoo, Bing (MSN). Searches open your browser on explicit request only.
)TXT");}
inline QString urlFor(const QString &engine,const QString &query){
    const QString k=engine.toLower();
    QString host;
    if(k=="duckduckgo"||k=="ddg")host="https://duckduckgo.com/?q=";
    else if(k=="google")host="https://www.google.com/search?q=";
    else if(k=="yahoo")host="https://search.yahoo.com/search?p=";
    else if(k=="bing"||k=="msn")host="https://www.bing.com/search?q=";
    else if(k=="brave")host="https://search.brave.com/search?q=";
    if(host.isEmpty()||query.trimmed().isEmpty()||query.size()>300)return {};
    return host+QString::fromLatin1(QUrl::toPercentEncoding(query));
}
inline QString blogTemplate(const QString &topic){
    const QString safe=topic.trimmed().isEmpty()?"family history research":topic.trimmed().left(160);
    return "BLOG (draft; edit facts before publishing)\nTitle: Exploring Our Family History\n\n"
           "Paragraph 1: This research update examines "+safe+" using documented sources. The goal is to make genealogical findings transparent, accurate, and easy to explore, with links to primary records where permitted.\n\n"
           "Paragraph 2: The next stage will compare those records with existing family connections, distinguish verified information from open questions, and invite evidence-based collaboration. Read the supporting sources and follow future updates on j03.page.\n\n"
           "FACEBOOK: New research on "+safe+". Read the latest documented findings and open questions on j03.page: [BLOG_URL]\n\n"
           "LINKEDIN: A new genealogy research update examines "+safe+" with a focus on source verification and documented connections. Full post: [BLOG_URL]\n\n"
           "COWLES MOUNTAIN SPEECH (about 35-40 seconds): Hello from Cowles Mountain. Today I've been exploring "+safe+" and documenting what the evidence supports. Family history is more than a list of names. It's a way to connect people, places, and records responsibly. You can read the new update at jay zero three dot page. Thanks for following along, and I'll share more as the research develops.\n\n"
           "Review names, links, source accuracy, privacy and actual SEO wording before sharing.";
}
inline QString help(){return "POWER | POWER STATUS | POWER MODELS | POWER GUIDE | POWER BLOG <topic> | POWER SEARCH <ddg/google/yahoo/bing/brave> <query> | POWER HELP";}
inline void install(QMainWindow *window,QTabWidget *tabs){
    auto *page=new QWidget(tabs);auto *lay=new QVBoxLayout(page);
    auto *intro=new QLabel("Power Up | Check local AI models and computer resources. Plan research and 4-word blog titles, two paragraphs, SEO-friendly copy, Facebook/LinkedIn updates and Cowles Mountain speech drafts.",page);
    intro->setWordWrap(true);lay->addWidget(intro);
    auto *metrics=new QLabel(page);metrics->setWordWrap(true);metrics->setTextInteractionFlags(Qt::TextSelectableByMouse);lay->addWidget(metrics);
    auto *actions=new QHBoxLayout;lay->addLayout(actions);
    auto *refresh=new QPushButton("Refresh resource checks",page);actions->addWidget(refresh);
    auto *check=new QPushButton("Check installed AI models",page);actions->addWidget(check);
    auto *guideButton=new QPushButton("Installation & Vibe wiring guide",page);actions->addWidget(guideButton);
    auto *output=new QPlainTextEdit(page);output->setReadOnly(true);lay->addWidget(output,1);
    auto *searchRow=new QHBoxLayout;lay->addLayout(searchRow);
    auto *engines=new QComboBox(page);engines->addItems({"DuckDuckGo","Google","Yahoo","Bing","Brave"});searchRow->addWidget(engines);
    auto *query=new QLineEdit(page);query->setPlaceholderText("Public search query (never paste private family records)");searchRow->addWidget(query,1);
    auto *openSearch=new QPushButton("Search in browser",page);searchRow->addWidget(openSearch);
    auto *blogRow=new QHBoxLayout;lay->addLayout(blogRow);
    auto *topic=new QLineEdit(page);topic->setPlaceholderText("Blog topic (draft only)");blogRow->addWidget(topic,1);
    auto *draft=new QPushButton("Create blog & social draft",page);blogRow->addWidget(draft);
    auto *last=new Cpu(cpuSample());
    QObject::connect(page,&QObject::destroyed,[last]{delete last;});
    auto *timer=new QTimer(page);timer->setInterval(2000);
    auto refreshStatus=[=]{Cpu next=cpuSample();double cpu=cpuPercent(*last,next);*last=next;metrics->setText(health(cpu));};
    QObject::connect(timer,&QTimer::timeout,page,refreshStatus);timer->start();
    QObject::connect(refresh,&QPushButton::clicked,page,refreshStatus);
    QObject::connect(guideButton,&QPushButton::clicked,page,[=]{output->setPlainText(guide());});
    QObject::connect(check,&QPushButton::clicked,page,[=]{
        QString exe=QStandardPaths::findExecutable("ollama");
        if(exe.isEmpty()){output->setPlainText("Ollama executable unavailable in PATH.\n"+guide());return;}
        check->setEnabled(false);
        auto *p=new QProcess(page);
        auto *timeout=new QTimer(p);timeout->setSingleShot(true);
        QObject::connect(timeout,&QTimer::timeout,p,[p]{if(p->state()!=QProcess::NotRunning)p->kill();});
        QObject::connect(p,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),page,[=](int code,QProcess::ExitStatus es){
            QString listing=QString::fromUtf8(p->readAllStandardOutput());
            QString errors=QString::fromUtf8(p->readAllStandardError());
            output->setPlainText(code==0&&es==QProcess::NormalExit ? modelReport(listing)+"\n\n"+listing.left(5000) : "Ollama list failed: "+errors.left(1500));
            check->setEnabled(true);p->deleteLater();
        });
        QObject::connect(p,&QProcess::errorOccurred,page,[=](QProcess::ProcessError){output->setPlainText(p->errorString());check->setEnabled(true);});
        p->start(exe,{"list"});timeout->start(5000);
    });
    QObject::connect(openSearch,&QPushButton::clicked,page,[=]{
        QString u=urlFor(engines->currentText(),query->text());
        if(u.isEmpty()){output->setPlainText("Enter a supported search engine and nonempty public query.");return;}
        output->setPlainText(QDesktopServices::openUrl(QUrl(u))?"Opened browser search: "+u:"Could not open browser search URL.");
    });
    QObject::connect(draft,&QPushButton::clicked,page,[=]{output->setPlainText(blogTemplate(topic->text()));});
    tabs->addTab(page,"Power Up");
    // Visible controls continue to work through the existing WIDGETS/CLICK/SET terminal bridge.
    Q_UNUSED(window);
}
inline bool handle(const QString &line,QPointer<QTcpSocket> socket,const std::function<void(const QString&)> &send){
    Q_UNUSED(socket)
    if(line.compare("POWER",Qt::CaseInsensitive)==0||line.compare("POWER HELP",Qt::CaseInsensitive)==0){send(help());return true;}
    if(!line.startsWith("POWER ",Qt::CaseInsensitive))return false;
    const QString cmd=line.mid(6).trimmed();
    if(cmd.compare("STATUS",Qt::CaseInsensitive)==0){
        // Synchronous one-second CPU sample would block the GUI; terminal snapshot reports other metrics.
        send(health(-1)+"\nCPU: use Power Up GUI live sampler (2s refresh).");return true;
    }
    if(cmd.compare("GUIDE",Qt::CaseInsensitive)==0){send(guide());return true;}
    if(cmd.compare("MODELS",Qt::CaseInsensitive)==0){
        const QString exe=QStandardPaths::findExecutable("ollama");
        if(exe.isEmpty()){send("Ollama unavailable in PATH. POWER GUIDE for installation instructions.");return true;}
        auto *p=new QProcess(socket.data());p->setProcessChannelMode(QProcess::SeparateChannels);
        auto *timeout=new QTimer(p);timeout->setSingleShot(true);
        QObject::connect(timeout,&QTimer::timeout,p,[p]{if(p->state()!=QProcess::NotRunning)p->kill();});
        QObject::connect(p,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),p,[=](int exitCode,QProcess::ExitStatus es){
            QString out=QString::fromUtf8(p->readAllStandardOutput());QString err=QString::fromUtf8(p->readAllStandardError());
            if(socket && socket->state()==QAbstractSocket::ConnectedState)socket->write((exitCode==0&&es==QProcess::NormalExit?modelReport(out):"Ollama list failed: "+err.left(1000)).toUtf8()+"\n");
            p->deleteLater();
        });
        QObject::connect(p,&QProcess::errorOccurred,p,[=](QProcess::ProcessError){if(socket)socket->write(("Ollama error: "+p->errorString()+"\n").toUtf8());});
        p->start(exe,{"list"});timeout->start(5000);send("Checking installed models...");return true;
    }
    if(cmd.startsWith("BLOG ",Qt::CaseInsensitive)){send(blogTemplate(cmd.mid(5)));return true;}
    if(cmd.startsWith("SEARCH ",Qt::CaseInsensitive)){
        QString rest=cmd.mid(7).trimmed();int pos=rest.indexOf(' ');
        if(pos<1){send("Usage: POWER SEARCH google <public query>");return true;}
        QString u=urlFor(rest.left(pos),rest.mid(pos+1));
        if(u.isEmpty()){send("Unknown engine or invalid search query");return true;}
        send(QDesktopServices::openUrl(QUrl(u))?"Opened browser search: "+u:"Could not open browser");return true;
    }
    send(help());return true;
}
} // namespace ATEPower

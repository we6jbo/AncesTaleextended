#pragma once
// In-place extension for AncesTaleextended. No authoritative family-data writes.
#include <QtWidgets>
#include <QDesktopServices>
#include <functional>
#include <memory>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QTcpSocket>
#include <QPointer>
#include <QStandardPaths>

namespace ATEMission {
inline const QString helperPath(){
    return QStringLiteral("/home/we6jbo/Projects/AncesTaleextended/mission_bridge.py");
}
inline QString statePath(){return QDir::homePath()+"/.local/share/AncesTaleextended/mission";}
inline QString pretty(const QJsonObject &obj){return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));}
inline void run(QObject *owner,const QStringList &args,std::function<void(QJsonObject,QString)> done){
    auto *p=new QProcess(owner);
    p->setProgram("python3");p->setArguments(QStringList{helperPath()}+args);
    QObject::connect(p,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),owner,[p,done](int exit,QProcess::ExitStatus){
        auto data=p->readAllStandardOutput(); QJsonParseError err;
        auto json=QJsonDocument::fromJson(data,&err);
        QString error;
        if(exit!=0)error=QString::fromUtf8(p->readAllStandardError()).trimmed();
        if(!json.isObject())error+=" Invalid helper response: "+QString::fromUtf8(data.left(500));
        if(json.isObject()&&json.object().contains("error"))error=json.object().value("error").toString();
        done(json.object(),error.trimmed()); p->deleteLater();
    });
    QObject::connect(p,&QProcess::errorOccurred,owner,[p,done](QProcess::ProcessError){
        // QProcess::finished may still follow. Only report error upon completion.
        Q_UNUSED(done);Q_UNUSED(p);
    });
    p->start();
}
class Label:public QGraphicsTextItem {
public:
    Label(const QString &text,QGraphicsItem *parent=nullptr):QGraphicsTextItem(text,parent){
        setFlag(QGraphicsItem::ItemIsMovable,true);setFlag(QGraphicsItem::ItemSendsGeometryChanges,true);
        setDefaultTextColor(Qt::black);setTextWidth(235);
    }
};
struct Line {Label *a;Label *b;QGraphicsLineItem *connector;};
inline void updateLines(const QList<Line> &lines){
    for(const auto &line:lines){
        auto ar=line.a->sceneBoundingRect();auto br=line.b->sceneBoundingRect();
        QLineF updated(ar.right()+3,ar.center().y(),br.left()-3,br.center().y());
        if(line.connector->line()!=updated)line.connector->setLine(updated);
    }
}
inline const QVector<QPair<QString,QString>>& goals(){
    static QVector<QPair<QString,QString>> g={
        {"Mary Johannsen","Hinrich Knuttel"},{"William L Schorf","Fredericka Schlorff"},
        {"Olive G Stafford","Sarah Greene"},{"William Shapley","Hans Bar"},
        {"Mary Rice","Margaret Ellis"},{"Amelia Kiehlbauch","Blandina Stoeffler"},
        {"Robert R Hedtke","Friedrich Wilhelm Luedtke"},{"Gail Agnes Hillman","David C Platt"},
        {"William R Maynard","William Cline"},{"Nancy Jane Lambert","Tabitha Brown"},
        {"Thomas S Bowman","Jacob Craven"},{"Rose Ann Prickett","Jesse Shoemaker"},
        {"Albert Franklin Smith","Elizabeth Goodspeed"},{"Elizabeth McQuistion","Mary Thorpe"},
        {"James N Elliott","Margaret Scott"},{"Sarah McAvoy","Mary Evett"},
        {"Edward McCabe","John McCabe"}};
    return g;
}
inline bool storeLayout(const QList<Line> &lines){
    QDir().mkpath(statePath());QJsonArray arr;
    for(const auto &l:lines){QJsonObject o;
        o["left_x"]=l.a->pos().x();o["left_y"]=l.a->pos().y();
        o["right_x"]=l.b->pos().x();o["right_y"]=l.b->pos().y();arr.append(o);
    }
    QSaveFile f(statePath()+"/mission-layout.json");
    if(!f.open(QIODevice::WriteOnly))return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));return f.commit();
}
inline void install(QWidget *owner,QTabWidget *tabs){
    // This feature is created after the original tabs, within the main tab layout.
    auto *quest=new QWidget(tabs);auto *outer=new QVBoxLayout(quest);
    auto *heading=new QLabel("17 Family Connection Quests: only documented paths count as completed");
    heading->setWordWrap(true);outer->addWidget(heading);
    auto *progress=new QProgressBar(quest);progress->setRange(0,17);outer->addWidget(progress);
    auto *score=new QLabel("Calculating documented relationships...");outer->addWidget(score);
    auto *controls=new QHBoxLayout;auto *refresh=new QPushButton("Recalculate paths");
    auto *smart=new QPushButton("Smart Layout");auto *save=new QPushButton("Save layout");
    controls->addWidget(refresh);controls->addWidget(smart);controls->addWidget(save);outer->addLayout(controls);
    auto *view=new QGraphicsView(quest);auto *scene=new QGraphicsScene(view);view->setScene(scene);
    view->setRenderHint(QPainter::Antialiasing,true);
    view->setMinimumHeight(370);outer->addWidget(view,2);
    auto *details=new QPlainTextEdit(quest);details->setReadOnly(true);details->setPlaceholderText("Select a goal for its documented path and relationship types.");details->setMaximumHeight(155);outer->addWidget(details);
    auto *table=new QTableWidget(17,3,quest);table->setHorizontalHeaderLabels({"Goal","Connection","Result"});
    table->horizontalHeader()->setStretchLastSection(true);table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setMinimumHeight(160);outer->addWidget(table,1);
    QList<Line> lines;
    QFile layoutFile(statePath()+"/mission-layout.json");QJsonArray layoutData;
    if(layoutFile.open(QIODevice::ReadOnly)){auto d=QJsonDocument::fromJson(layoutFile.readAll());if(d.isArray())layoutData=d.array();}
    for(int i=0;i<goals().size();++i){
        const auto &g=goals()[i];auto *a=new Label(g.first);auto *b=new Label(g.second);
        QFont font=a->font();font.setPointSize(10);a->setFont(font);b->setFont(font);
        scene->addItem(a);scene->addItem(b);
        const QJsonObject saved=i<layoutData.size()?layoutData[i].toObject():QJsonObject();
        a->setPos(saved.value("left_x").toDouble(12),saved.value("left_y").toDouble(i*35));
        b->setPos(saved.value("right_x").toDouble(580),saved.value("right_y").toDouble(i*35));
        auto *connector=scene->addLine(QLineF(),QPen(QColor("#16cf65"),1.5));connector->setZValue(-1);
        lines.append({a,b,connector});
        table->setItem(i,0,new QTableWidgetItem(QString::number(i+1)));
        table->setItem(i,1,new QTableWidgetItem(g.first+"  →  "+g.second));
        table->setItem(i,2,new QTableWidgetItem("Not yet evaluated"));
    }
    updateLines(lines);scene->setSceneRect(0,0,850,630);
    auto *layoutTimer=new QTimer(quest);layoutTimer->setSingleShot(true);layoutTimer->setInterval(650);
    QObject::connect(scene,&QGraphicsScene::changed,quest,[=]{
        updateLines(lines);if(!layoutTimer->isActive())layoutTimer->start();
    });
    QObject::connect(layoutTimer,&QTimer::timeout,quest,[=]{storeLayout(lines);});
    QObject::connect(save,&QPushButton::clicked,quest,[=]{storeLayout(lines);});
    QObject::connect(smart,&QPushButton::clicked,quest,[=]{
        for(int i=0;i<lines.size();++i){lines[i].a->setPos(12,i*35);lines[i].b->setPos(580,i*35);}updateLines(lines);storeLayout(lines);
    });
    auto results=std::make_shared<QJsonArray>();
    QObject::connect(table,&QTableWidget::cellClicked,quest,[=](int row,int){
        if(row<0||row>=results->size())return;auto x=(*results)[row].toObject();
        details->setPlainText(pretty(x));
    });
    auto recalc=[=]{
        refresh->setEnabled(false);
        run(quest,{"quests"},[=](QJsonObject data,QString error){
            refresh->setEnabled(true);
            if(!error.isEmpty()){score->setText("Quest evaluation failed: "+error);return;}
            int complete=data.value("completed").toInt();
            progress->setValue(complete);
            score->setText(QString("%1/%2 discovered (%3%). %4 remaining. Unverified edges excluded: %5")
                .arg(complete).arg(17).arg(data.value("percentage").toDouble(),0,'f',2)
                .arg(17-complete).arg(data.value("excluded_uncertain_edges").toInt()));
            *results=data.value("goals").toArray();
            for(int i=0;i<results->size() && i<lines.size();++i){
                auto g=(*results)[i].toObject();bool success=g.value("completed").toBool();
                lines[i].connector->setPen(QPen(success?Qt::white:QColor("#16cf65"),success?3.5:1.5));
                table->item(i,2)->setText(success?"KOWABUNGA! Verified path":g.value("status").toString());
                table->item(i,2)->setForeground(success?QColor("#168c32"):QColor("#a56a10"));
            }
        });
    };
    QObject::connect(refresh,&QPushButton::clicked,quest,recalc);
    tabs->addTab(quest,"Family Connection Quest");
    auto *site=new QWidget(tabs);auto *sl=new QVBoxLayout(site);
    auto *guide=new QLabel("Website research submissions are unverified leads, not confirmed ancestors. SSH read only; remote audit.php is never modified.");
    guide->setWordWrap(true);sl->addWidget(guide);
    auto *row=new QHBoxLayout;auto *fetch=new QPushButton("Fetch research via SSH");
    auto *local=new QPushButton("Show saved leads");auto *openSite=new QPushButton("Open family website");
    row->addWidget(fetch);row->addWidget(local);row->addWidget(openSite);sl->addLayout(row);
    auto *search=new QLineEdit;search->setPlaceholderText("Filter submissions by person, TG ID, or notes");sl->addWidget(search);
    auto *leads=new QTableWidget(site);leads->setColumnCount(5);
    leads->setHorizontalHeaderLabels({"Submission ID","Person","Associated TG","Submitted","Review"});
    leads->horizontalHeader()->setStretchLastSection(true);leads->setSelectionBehavior(QAbstractItemView::SelectRows);
    leads->setEditTriggers(QAbstractItemView::NoEditTriggers);sl->addWidget(leads,3);
    auto *leadDetails=new QPlainTextEdit(site);leadDetails->setReadOnly(true);sl->addWidget(leadDetails,2);
    auto *reviewRow=new QHBoxLayout;auto *review=new QComboBox(site);
    review->addItems({"unverified","reviewed","accepted","rejected","more-evidence"});
    auto *apply=new QPushButton("Save review status (local only)");
    reviewRow->addWidget(review);reviewRow->addWidget(apply);sl->addLayout(reviewRow);
    auto *siteStatus=new QLabel("No remote records have been fetched.");siteStatus->setWordWrap(true);sl->addWidget(siteStatus);
    auto entries=std::make_shared<QJsonArray>();
    auto render=[=]{
        QString term=search->text().trimmed();leads->setRowCount(0);
        for(const auto &v:*entries){auto x=v.toObject();auto p=x.value("record").toObject();
            QString name=p.value("first_name").toString()+" "+p.value("last_name").toString();
            QString summary=name+" "+p.value("line_tg_id").toString()+" "+p.value("notes").toString();
            if(!summary.contains(term,Qt::CaseInsensitive))continue;
            int r=leads->rowCount();leads->insertRow(r);
            QStringList cols={x.value("id").toString(),name.trimmed(),p.value("line_tg_id").toString(),
                              x.value("submitted_at").toString(),x.value("status").toString()};
            for(int j=0;j<cols.size();j++)leads->setItem(r,j,new QTableWidgetItem(cols[j]));
        }
    };
    QObject::connect(search,&QLineEdit::textChanged,site,render);
    QObject::connect(leads,&QTableWidget::itemSelectionChanged,site,[=]{
        auto selected=leads->selectedItems();if(selected.isEmpty())return;
        auto id=leads->item(selected[0]->row(),0)->text();
        for(auto v:*entries){auto x=v.toObject();if(x.value("id").toString()==id){leadDetails->setPlainText(pretty(x));review->setCurrentText(x.value("status").toString());break;}}
    });
    auto reload=[=]{
        run(site,{"leads"},[=](QJsonObject obj,QString error){
            if(!error.isEmpty()){siteStatus->setText(error);return;}
            *entries=obj.value("leads").toArray();render();
            siteStatus->setText(QString("%1 locally reviewed/unverified submissions").arg(entries->size()));
        });
    };
    QObject::connect(local,&QPushButton::clicked,site,reload);
    QObject::connect(fetch,&QPushButton::clicked,site,[=]{
        fetch->setEnabled(false);siteStatus->setText("Fetching over SSH (read only)...");
        run(site,{"fetch-leads"},[=](QJsonObject obj,QString error){
            fetch->setEnabled(true);
            if(!error.isEmpty()){siteStatus->setText(error);return;}
            siteStatus->setText(pretty(obj));reload();
        });
    });
    QObject::connect(apply,&QPushButton::clicked,site,[=]{
        auto selected=leads->selectedItems();if(selected.isEmpty())return;
        QString id=leads->item(selected[0]->row(),0)->text();
        run(site,{"review",id,review->currentText()},[=](QJsonObject obj,QString error){
            siteStatus->setText(error.isEmpty()?pretty(obj):error);if(error.isEmpty())reload();
        });
    });
    QObject::connect(openSite,&QPushButton::clicked,site,[]{
        QDesktopServices::openUrl(QUrl("http://100.73.131.89:8080/family/"));
    });
    tabs->addTab(site,"Website Research");
    recalc();
}

// Terminal extension: only called by the existing authenticated port 53141 server.
template<class Send>
inline bool handle(const QString &line,QObject *owner, Send send){
    const QString input=line.trimmed();
    if(input.compare("QUEST",Qt::CaseInsensitive)==0 || input.compare("QUEST HELP",Qt::CaseInsensitive)==0){
        send("QUEST commands: QUEST SCORE | QUEST LIST | QUEST COMPLETED | QUEST MISSING | QUEST RECENT | QUEST SHOW <1-17> | QUEST FIND <name> | QUEST HELP");
        return true;
    }
    if(!input.startsWith("QUEST ",Qt::CaseInsensitive) && input.compare("QUEST",Qt::CaseInsensitive)!=0)return false;
    const QString action=input.mid(6).trimmed();
    if(action.compare("RECENT",Qt::CaseInsensitive)==0){
        run(owner,{"announce"},[=](QJsonObject obj,QString err){
            if(!err.isEmpty()){send("QUEST ERROR: "+err);return;}
            for(auto v:obj.value("new").toArray()){
                auto x=v.toObject();send("KOWABUNGA! "+x.value("left").toString()+" -> "+x.value("right").toString());
            }
            send(QString("New discoveries: %1").arg(obj.value("new").toArray().size()));
        });return true;
    }
    run(owner,{"quests"},[=](QJsonObject obj,QString err){
        if(!err.isEmpty()){send("QUEST ERROR: "+err);return;}
        auto goals=obj.value("goals").toArray();
        if(action.compare("SCORE",Qt::CaseInsensitive)==0){
            send(QString("Quest progress: %1/%2 (%3%)").arg(obj.value("completed").toInt())
                 .arg(obj.value("total").toInt()).arg(obj.value("percentage").toDouble(),0,'f',2));return;
        }
        if(action.compare("LIST",Qt::CaseInsensitive)==0 || action.compare("COMPLETED",Qt::CaseInsensitive)==0 || action.compare("MISSING",Qt::CaseInsensitive)==0){
            for(auto v:goals){auto x=v.toObject();bool completed=x.value("completed").toBool();
                if(action.compare("COMPLETED",Qt::CaseInsensitive)==0&&!completed)continue;
                if(action.compare("MISSING",Qt::CaseInsensitive)==0&&completed)continue;
                send(QString("%1. %2 -> %3 [%4]").arg(x.value("id").toInt()).arg(x.value("left").toString())
                     .arg(x.value("right").toString()).arg(x.value("status").toString()));
            }return;
        }
        if(action.startsWith("SHOW ",Qt::CaseInsensitive)){
            bool ok=false;int index=action.mid(5).toInt(&ok);
            if(!ok||index<1||index>goals.size()){send("Enter QUEST SHOW 1 through 17");return;}
            send(pretty(goals[index-1].toObject()));return;
        }
        if(action.startsWith("FIND ",Qt::CaseInsensitive)){
            QString query=action.mid(5).trimmed();int found=0;
            for(auto v:goals){auto x=v.toObject();
                if((x.value("left").toString()+" "+x.value("right").toString()).contains(query,Qt::CaseInsensitive)){
                    send(QString("%1. %2 -> %3: %4").arg(x.value("id").toInt()).arg(x.value("left").toString())
                         .arg(x.value("right").toString()).arg(x.value("status").toString()));found++;
                }
            }if(!found)send("No matching quest labels");return;
        }
        send("Unknown QUEST command. Enter QUEST HELP");
    });
    return true;
}
}

#pragma once
#include "mission_tabs.h"
#include "power_up.h"
#include "classical_network.h"
// In-process, authenticated loopback terminal bridge for the running Qt GUI.
// This file is not a standalone remote-desktop server.
#include <QtWidgets>
#include "dialog_tab.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <memory>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>
#include <QPointer>

namespace ATEControl {
constexpr quint16 port = 53141;
static const QString kAudit = QDir::homePath()+"/Projects/AncesTaleextended/audits-for-misunderstood-info.txt";
static const QString kToken = QDir::homePath()+"/.config/AncesTaleextended/control-token";

inline QString tokenFile(){
    QFileInfo info(kToken);
    if(info.isSymLink() || (info.exists() && !info.isFile()))return {};
    if(info.isFile()){
        QFile file(kToken);if(!file.open(QIODevice::ReadOnly))return {};
        const auto v=QString::fromUtf8(file.readAll()).trimmed();
        if(v.size()<48)return {};
        file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
        return v;
    }
    if(!QDir().mkpath(QFileInfo(kToken).absolutePath()))return {};
    QByteArray bytes;
    for(int i=0;i<8;i++){quint32 n=QRandomGenerator::system()->generate();
        for(int j=0;j<4;j++)bytes.append(char((n>>(j*8)) & 0xff));}
    const auto value=QString::fromLatin1(bytes.toHex());
    QSaveFile f(kToken);
    if(!f.open(QIODevice::WriteOnly) || f.write(value.toUtf8()+"\n")<0 || !f.commit())return {};
    QFile::setPermissions(kToken,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    return value;
}

inline bool writeAudit(const QString &content,const QString &answers,const QString &assessment){
    const QFileInfo info(kAudit);
    if(info.isSymLink())return false;
    QFile f(kAudit);
    if(!f.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text))return false;
    f.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    QString event = QString("\n[%1] Source report: %2\nAnswers: %3\nAssessment: %4\n")
      .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate),content,answers,assessment);
    return f.write(event.toUtf8())==event.toUtf8().size();
}

struct State {
    bool authorized=false;
    int classicalApp=0;
    bool verbose=true;
    QString lastDialog;
    int auditStage=0;
    QString report;
    QStringList answers;
};

inline QString notesPath(){return QDir::homePath()+"/.local/share/AncesTaleextended/terminal-notes.sqlite";}
inline QString notesKey(const QTabWidget* tabs, const QString& kind, QWidget* control=nullptr){
    const QString tab=tabs->tabText(tabs->currentIndex());
    if(kind=="tab")return "tab:"+tab;
    if(kind=="dialog")return "dialog:"+(control?control->objectName():QString());
    if(!control)return {};
    QString label=control->objectName();
    if(label.isEmpty()){
        if(auto b=qobject_cast<QAbstractButton*>(control))label=b->text();
        if(label.isEmpty())label=QString::fromLatin1(control->metaObject()->className());
    }
    return "control:"+tab+":"+label;
}
inline QList<QWidget*> currentControls(QTabWidget* tabs){
    QList<QWidget*> out;
    QWidget* page=tabs->currentWidget();
    if(!page)return out;
    for(auto* w:page->findChildren<QWidget*>()){
        if(!w->isVisible() || !w->isEnabled())continue;
        if(qobject_cast<QAbstractButton*>(w)||qobject_cast<QLineEdit*>(w)||qobject_cast<QComboBox*>(w)||qobject_cast<QSpinBox*>(w)||(qobject_cast<QPlainTextEdit*>(w)&&!qobject_cast<QPlainTextEdit*>(w)->isReadOnly()))out<<w;
    }
    return out;
}
inline QString controlName(QWidget* w){
    if(auto b=qobject_cast<QAbstractButton*>(w))return b->text().trimmed().isEmpty()?"Button":b->text().trimmed();
    if(auto c=qobject_cast<QComboBox*>(w))return c->objectName().isEmpty()?"Combo box":c->objectName();
    if(qobject_cast<QLineEdit*>(w))return "Text field";
    if(auto p=qobject_cast<QPlainTextEdit*>(w))return p->objectName().isEmpty()?"Notes / draft editor":p->objectName();
    if(qobject_cast<QSpinBox*>(w))return "Spin box";
    return w->objectName();
}
inline QString explain(QWidget* w){
    QString name=controlName(w);
    if(auto b=qobject_cast<QAbstractButton*>(w)){
        if(!b->toolTip().isEmpty())return b->toolTip();
        if(QRegularExpression("publish|push|upload",QRegularExpression::CaseInsensitiveOption).match(name).hasMatch())return "Starts a publication action; confirm content and required authorization in GUI";
        if(QRegularExpression("delete|remove",QRegularExpression::CaseInsensitiveOption).match(name).hasMatch())return "Potentially destructive action; review before confirming";
        return "Activates the matching GUI control";
    }
    if(qobject_cast<QLineEdit*>(w)||qobject_cast<QPlainTextEdit*>(w))return "Changes this GUI text field (values are not logged)";
    if(qobject_cast<QSpinBox*>(w))return "Sets a numeric GUI value";
    if(qobject_cast<QComboBox*>(w))return "Changes the selected GUI item";
    return "Changes a GUI value";
}
inline bool safeNotesText(const QString& t){return !t.trimmed().isEmpty() && t.size()<=4000 && !t.contains(QChar::Null);}

inline void install(QMainWindow* window,QTabWidget* tabs){
    auto *page=new QWidget(tabs);
    auto *layout=new QVBoxLayout(page);
    auto *status=new QLabel(page);
    status->setWordWrap(true);
    auto *activity=new QPlainTextEdit(page);
    activity->setReadOnly(true);
    layout->addWidget(new QLabel("Authenticated local terminal control | 127.0.0.1:53141",page));
    layout->addWidget(status);
    layout->addWidget(activity,1);
    tabs->addTab(page,"Terminal control");
    auto log=[activity](const QString &s){activity->appendPlainText(QDateTime::currentDateTime().toString(Qt::ISODate)+" "+s);};
    const QString token=tokenFile();
    if(token.isEmpty()){
        status->setText("Disabled: secure local authentication token unavailable.");return;
    }
    auto *server=new QTcpServer(page);
    if(!server->listen(QHostAddress::LocalHost,port)){
        status->setText("Listener unavailable: "+server->errorString());return;
    }
    status->setText("Listening on 127.0.0.1:53141. Token at ~/.config/AncesTaleextended/control-token (owner only). Connect with nc 127.0.0.1 53141.");
    log("Local terminal listener started; authentication required");
    QObject::connect(server,&QTcpServer::newConnection,page,[=]{
        while(server->hasPendingConnections()){
            QTcpSocket *socket=server->nextPendingConnection();
            socket->setParent(page);
            auto state=std::make_shared<State>();
            auto send=[socket](const QString &message){if(socket->state()==QAbstractSocket::ConnectedState)socket->write(message.toUtf8()+"\n");};
            send("AncesTaleextended control bridge. Authenticate using: AUTH <token>. The token is stored at ~/.config/AncesTaleextended/control-token. No commands are available before authentication.");
            QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            const QString connName="ate-notes-"+QString::number(quintptr(socket),16);
            QDir().mkpath(QFileInfo(notesPath()).absolutePath());
            QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",connName);
            db.setDatabaseName(notesPath());
            const bool dbReady=!QFileInfo(notesPath()).isSymLink() && db.open() && QSqlQuery(db).exec("CREATE TABLE IF NOT EXISTS notes (id INTEGER PRIMARY KEY AUTOINCREMENT, scope TEXT NOT NULL, content TEXT NOT NULL, created_utc TEXT NOT NULL)");
            if(dbReady)QFile::setPermissions(notesPath(),QFileDevice::ReadOwner|QFileDevice::WriteOwner);
            auto listNotes=[=](const QString &scope){
                if(!dbReady){send("Notes storage unavailable");return;}
                QSqlQuery q(db);
                q.prepare("SELECT id,created_utc,content FROM notes WHERE scope=? ORDER BY id");q.addBindValue(scope);
                if(!q.exec()){send("Notes read failed");return;}
                bool any=false;
                while(q.next()){any=true;send(QString("NOTE %1 [%2]: %3").arg(q.value(0).toString(),q.value(1).toString(),q.value(2).toString()));}
                if(!any)send("No notes for "+scope);
            };
            auto showCurrent=[=](bool detail){
                send(QString("TAB %1: %2").arg(tabs->currentIndex()).arg(tabs->tabText(tabs->currentIndex())));
                auto controls=currentControls(tabs);
                if(controls.isEmpty())send("No available controls on this tab");
                for(int i=0;i<controls.size();i++){
                    QWidget* w=controls[i];
                    QString verb=qobject_cast<QAbstractButton*>(w)?"CLICK":qobject_cast<QComboBox*>(w)?"SELECT":qobject_cast<QLineEdit*>(w)?"SET":"SET";
                    send(QString("%1 %2 - %3").arg(verb).arg(i+1).arg(controlName(w)));
                    if(detail){send("  "+explain(w));listNotes(notesKey(tabs,"control",w));}
                }
                if(detail){send("NOTES FOR CURRENT TAB:");listNotes(notesKey(tabs,"tab"));}
            };
            auto activeDialog=[=]()->QMessageBox*{
                auto* modal=QApplication::activeModalWidget();
                return qobject_cast<QMessageBox*>(modal);
            };
            auto showDialog=[=](QMessageBox* dlg,bool detail){
                if(ATEDialog::active(tabs)){
                    send("DIALOG TAB: "+ATEDialog::title(tabs));
                    send(ATEDialog::text(tabs));
                    send("CLICK 1 / YES: confirm | CLICK 2 / NO: cancel");
                    if(detail)listNotes("dialog:"+ATEDialog::title(tabs));
                    return;
                }
                if(!dlg){send("No supported confirmation dialog is open");return;}
                send("DIALOG: "+dlg->windowTitle()+" | "+dlg->text());
                for(auto* b:dlg->buttons()){
                    QString role=QString::number(int(dlg->buttonRole(b)));
                    send("CHOICE: "+b->text()+" (role "+role+")");
                }
                if(detail){send("Use YES or NO for standard confirmation dialogs; protected remote-control approvals must be answered on the GUI.");listNotes("dialog:"+dlg->windowTitle());}
            };
            QObject::connect(tabs,&QTabWidget::currentChanged,socket,[=](int){
                if(!state->authorized || !state->verbose)return;
                QTimer::singleShot(0,socket,[=]{if(socket->state()==QAbstractSocket::ConnectedState){showCurrent(true);if(ATEDialog::active(tabs))showDialog(activeDialog(),true);}});
            });
            auto *dialogTimer=new QTimer(socket);
            dialogTimer->setInterval(220);
            QObject::connect(dialogTimer,&QTimer::timeout,socket,[=]{
                if(!state->authorized || !state->verbose)return;
                auto* dlg=activeDialog();
                QString id=ATEDialog::active(tabs)?("tab|"+ATEDialog::title(tabs)+"|"+ATEDialog::text(tabs)):(dlg?dlg->windowTitle()+"|"+dlg->text():QString());
                if(id==state->lastDialog)return;
                state->lastDialog=id;
                if(dlg || ATEDialog::active(tabs))showDialog(dlg,true);
            });
            dialogTimer->start();
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[=]{
                while(socket->canReadLine()){
                    QString line=QString::fromUtf8(socket->readLine()).trimmed();
                    if(state->authorized && state->auditStage==0 && ATEMission::handle(line, page, [socket](const QString &response){if(socket->state()==QAbstractSocket::ConnectedState)socket->write(response.toUtf8()+"\n");}))continue;
                    if(state->authorized && ATEClassicalNet::handle(line,state->classicalApp,QPointer<QTcpSocket>(socket),send))continue;
                    if(state->authorized && state->auditStage==0 && ATEPower::handle(line,QPointer<QTcpSocket>(socket),send))continue;
                    if(line.size()>8192){send("ERROR: command too long");continue;}
                    if(!state->authorized){
                        if(line.startsWith("AUTH ") && line.mid(5)==token){
                            state->authorized=true;
                            log("Authenticated local connection");
                            send("Privacy statement: The following birthdates are private and confidential: 03/24/1981, 12/31/1960, 09/16/1928, 06/05/1926. In addition .ogg files are private. All other family members are public and those public members must be placed on sites like moltbook, family tree dna, j03.page blog, etc. Publication still requires checks for other sensitive or living-person information and site permissions.");
                            send("Menu: 1. audit privacy statements | 2. control | HELP | QUIT");
                            send("Power Up available: POWER HELP | POWER STATUS | POWER MODELS | POWER GUIDE | POWER BLOG <topic> | POWER SEARCH <engine> <query>");
                            send("Classical AI services are available only here on port 53141. Enter AI for Moltbook, carventory, textwarden, and edutrace.");
                            send("Classical AI consultation: This connection can assist with family-tree research, completing missing family-tree information, and planning progress toward the ultimate family tree. Use TABS to find Classical or AI consultation; use OPTIONS for current-tab controls. Record usefulness feedback (1 difficult, 10 achieved goal) in AI consultation.");
                            send("Verbose ON (default). Use VERBOSE OFF for concise output. NOTES ADD <text> saves contextual notes.");
                            showCurrent(true);
                        } else {send("Authentication required. AUTH <token>");socket->disconnectFromHost();}
                        continue;
                    }
                    if(state->auditStage){
                        if(line.compare("CANCEL",Qt::CaseInsensitive)==0){state->auditStage=0;send("Audit canceled");continue;}
                        if(state->auditStage==1){
                            state->report=line;
                            state->auditStage=2;
                            send("Does the material contain any protected birthdate: 03/24/1981, 12/31/1960, 09/16/1928, or 06/05/1926? YES/NO");continue;
                        }
                        if(state->auditStage==3){
                            const QString ext=line.toUpper();
                            if(ext!="OGG" && ext!="MP3" && ext!="WAV" && ext!="NONE"){send("Enter OGG, MP3, WAV, or NONE (or CANCEL)");continue;}
                            state->answers<<ext;
                        }else{
                            if(line.compare("yes",Qt::CaseInsensitive)!=0 && line.compare("no",Qt::CaseInsensitive)!=0){send("Answer YES or NO, or CANCEL");continue;}
                            state->answers<<line.toUpper();
                        }
                        if(state->auditStage==2){state->auditStage=3;send("What is the file type? OGG, MP3, WAV, or NONE (only .ogg is categorically restricted by this rule)");continue;}
                        if(state->auditStage==3){state->auditStage=4;send("Does the material contain 1119 or joneal97? YES/NO");continue;}
                        const bool clear=state->answers.size()==3 && state->answers[0]=="NO" && state->answers[1]!="OGG" && state->answers[2]=="NO";
                        QString assessment=clear?"These three checks found no listed restriction; potentially public, subject to other privacy and source checks.":"Flagged for individual publication review; no automatic publication approved.";
                        QString report=state->report;
                        report.replace('\r',' ');report.replace('\n',' ');
                        const bool saved=writeAudit(report,state->answers.join(","),assessment);
                        send(saved?"Audit saved: "+assessment:"ERROR: unable to write audit record");
                        log("Privacy audit "+QString(saved?"saved":"failed"));
                        state->auditStage=0;state->answers.clear();state->report.clear();continue;
                    }
                    if(line.compare("1",Qt::CaseInsensitive)==0 || line.compare("audit",Qt::CaseInsensitive)==0){
                        state->auditStage=1;state->answers.clear();send("Enter the message, code, or content you believe was incorrectly restricted (one line). CANCEL to abort.");continue;
                    }
                    if(line.compare("2",Qt::CaseInsensitive)==0 || line.compare("control",Qt::CaseInsensitive)==0 || line.compare("HELP",Qt::CaseInsensitive)==0){
                        send("Commands: TABS | TAB n | OPTIONS | WIDGETS | CLICK n | SET n text | SELECT n item-index | CHECK n YES/NO | DIALOG | YES | NO | VERBOSE ON/OFF/STATUS | NOTES ADD/SHOW/EDIT/DELETE | NOTES ADD BUTTON n text | NOTES ADD DIALOG text | AUDIT | STATUS | BACK | AI (Moltbook/carventory/textwarden/edutrace) | QUIT");
                        if(state->verbose)showCurrent(true);
                        continue;
                    }
                    if(line.compare("QUIT",Qt::CaseInsensitive)==0){send("Bye");socket->disconnectFromHost();continue;}
                    if(line.compare("VERBOSE ON",Qt::CaseInsensitive)==0){state->verbose=true;send("OK: Verbose ON");if(state->verbose)showCurrent(true);if(activeDialog() || ATEDialog::active(tabs))showDialog(activeDialog(),true);continue;}
                    if(line.compare("VERBOSE OFF",Qt::CaseInsensitive)==0){state->verbose=false;send("OK: Verbose OFF");continue;}
                    if(line.compare("VERBOSE STATUS",Qt::CaseInsensitive)==0){send(state->verbose?"Verbose ON":"Verbose OFF");continue;}
                    if(line.compare("STATUS",Qt::CaseInsensitive)==0){send(QString("Current tab: %1 (%2) | Verbose: %3 | Dialog: %4").arg(tabs->currentIndex()).arg(tabs->tabText(tabs->currentIndex()),state->verbose?"ON":"OFF",ATEDialog::active(tabs)?ATEDialog::title(tabs):(activeDialog()?activeDialog()->windowTitle():"none")));continue;}
                    if(line.compare("TABS",Qt::CaseInsensitive)==0){
                        for(int i=0;i<tabs->count();++i)send(QString("TAB %1: %2").arg(i).arg(tabs->tabText(i)));
                        continue;
                    }
                    if(line.startsWith("TAB ",Qt::CaseInsensitive)){
                        bool ok=false;int n=line.mid(4).toInt(&ok);
                        if(!ok||n<0||n>=tabs->count()){send("Invalid tab index");continue;}
                        if(ATEDialog::active(tabs)&&tabs->widget(n)!=ATEDialog::session(tabs)->page){send("Answer the Dialog tab first: YES or NO");continue;}
                        tabs->setCurrentIndex(n);log(QString("Terminal selected tab %1: %2").arg(n).arg(tabs->tabText(n)));
                        send("OK: GUI tab changed");if(state->verbose)showCurrent(true);continue;
                    }
                    if(line.compare("OPTIONS",Qt::CaseInsensitive)==0 || line.compare("WIDGETS",Qt::CaseInsensitive)==0){showCurrent(state->verbose);continue;}
                    if(line.compare("DIALOG",Qt::CaseInsensitive)==0){showDialog(activeDialog(),true);continue;}
                    if(line.compare("BACK",Qt::CaseInsensitive)==0){
                        if(ATEDialog::reply(tabs,false)){send("Dialog canceled; returned to previous tab");continue;}
                        if(auto *d=activeDialog()){if(d->windowTitle()=="Remote control approval"){send("Remote action approval must be answered locally in the GUI");continue;} d->reject();send("Dialog canceled");}
                        else send("No active dialog to cancel");continue;
                    }
                    if(line.compare("YES",Qt::CaseInsensitive)==0 || line.compare("NO",Qt::CaseInsensitive)==0){
                        if(ATEDialog::active(tabs)){bool yes=line.compare("YES",Qt::CaseInsensitive)==0;ATEDialog::reply(tabs,yes);send(yes?"Dialog accepted; awaiting operation result":"Dialog canceled");continue;}
                        auto *d=activeDialog();if(!d){send("No active confirmation dialog");continue;}
                        if(d->windowTitle()=="Remote control approval"){send("Protected remote action requires local GUI owner approval");continue;}
                        bool yes=line.compare("YES",Qt::CaseInsensitive)==0;
                        auto *b=d->button(yes?QMessageBox::Yes:QMessageBox::No);
                        if(!b){send("This dialog does not support that choice; use DIALOG");continue;}
                        b->click();log("Terminal answered supported GUI dialog: "+line.toUpper());send("Dialog response sent; this is not proof the operation succeeded");continue;
                    }
                    if(line.compare("NOTES SHOW",Qt::CaseInsensitive)==0){listNotes(notesKey(tabs,"tab"));continue;}
                    if(line.startsWith("NOTES ",Qt::CaseInsensitive)){
                        if(!dbReady){send("Notes database unavailable");continue;}
                        QString rest=line.mid(6);
                        QString scope=notesKey(tabs,"tab"), content;
                        if(rest.startsWith("ADD BUTTON ",Qt::CaseInsensitive)){
                            QString tail=rest.mid(11);int blank=tail.indexOf(' ');bool ok=false;int n=tail.left(blank).toInt(&ok);
                            auto cs=currentControls(tabs);
                            if(blank<0||!ok||n<1||n>cs.size()){send("Usage: NOTES ADD BUTTON <current-control-number> <text>");continue;}
                            scope=notesKey(tabs,"control",cs[n-1]);content=tail.mid(blank+1);
                        } else if(rest.startsWith("ADD DIALOG ",Qt::CaseInsensitive)){
                            auto *d=activeDialog();if(!d){send("Open a dialog before attaching a dialog note");continue;}
                            scope="dialog:"+d->windowTitle();content=rest.mid(11);
                        } else if(rest.startsWith("ADD ",Qt::CaseInsensitive))content=rest.mid(4);
                        else if(rest.startsWith("EDIT ",Qt::CaseInsensitive)){
                            QString tail=rest.mid(5);int blank=tail.indexOf(' ');bool ok=false;int id=tail.left(blank).toInt(&ok);
                            content=blank>=0?tail.mid(blank+1):QString();
                            if(!ok||id<1||!safeNotesText(content)){send("Usage: NOTES EDIT <id> <text> (max 4000 chars)");continue;}
                            QSqlQuery q(db);q.prepare("UPDATE notes SET content=? WHERE id=?");q.addBindValue(content);q.addBindValue(id);
                            send(q.exec()&&q.numRowsAffected()==1?"Note edited":"Note ID not found or update failed");continue;
                        } else if(rest.startsWith("DELETE ",Qt::CaseInsensitive)){
                            bool ok=false;int id=rest.mid(7).trimmed().toInt(&ok);
                            if(!ok||id<1){send("Usage: NOTES DELETE <id>");continue;}
                            QSqlQuery q(db);q.prepare("DELETE FROM notes WHERE id=?");q.addBindValue(id);
                            send(q.exec()&&q.numRowsAffected()==1?"Note deleted":"Note ID not found or delete failed");continue;
                        } else {send("Usage: NOTES ADD <text> | NOTES SHOW | NOTES ADD BUTTON n <text> | NOTES ADD DIALOG <text> | NOTES EDIT id <text> | NOTES DELETE id");continue;}
                        if(!safeNotesText(content)){send("Note must contain 1 to 4000 characters");continue;}
                        QSqlQuery q(db);q.prepare("INSERT INTO notes(scope,content,created_utc) VALUES(?,?,?)");
                        q.addBindValue(scope);q.addBindValue(content);q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                        bool saved=q.exec();send(saved?"Note saved for "+scope:"ERROR: note not saved");
                        if(saved)log("Terminal note saved (text not logged)");continue;
                    }
                    auto controls=currentControls(tabs);
                    auto parts=line.split(' ',Qt::SkipEmptyParts);
                    if(parts.size()>=2 && (parts[0].compare("CLICK",Qt::CaseInsensitive)==0 || parts[0].compare("SET",Qt::CaseInsensitive)==0 || parts[0].compare("SELECT",Qt::CaseInsensitive)==0 || parts[0].compare("CHECK",Qt::CaseInsensitive)==0)){
                        bool ok=false;int n=parts[1].toInt(&ok);
                        if(!ok||n<1||n>controls.size()){send("Invalid current-tab control number. Enter OPTIONS to see available controls.");continue;}
                        QWidget *w=controls[n-1];
                        if(!w->isVisible()||!w->isEnabled()){send("Control is no longer available; enter OPTIONS");continue;}
                        if(parts[0].compare("CLICK",Qt::CaseInsensitive)==0){
                            auto *b=qobject_cast<QAbstractButton*>(w);
                            if(!b){send("Control is not a button. Enter OPTIONS");continue;}
                            const QString t=b->text();
                            if(QRegularExpression("publish|push|delete|remove|upload|commit|sync",QRegularExpression::CaseInsensitiveOption).match(t).hasMatch()){
                                QPointer<QAbstractButton> target(b);
                                ATEDialog::ask(tabs,"Remote control approval","Allow terminal control to activate: "+t+"?",
                                    [=](bool yes){if(yes && target && target->isEnabled()){
                                        target->click();log("Terminal activated approved GUI button: "+t);
                                    }else log("Terminal action canceled: "+t);});
                                send("Remote action needs confirmation. Dialog tab opened; enter YES or NO.");
                                continue;
                            }
                            b->click();log("Terminal clicked GUI button: "+t);send("Button invoked; check operation result");
                            if(state->verbose)showCurrent(true);continue;
                        }
                        if(parts[0].compare("SET",Qt::CaseInsensitive)==0){
                            int pos=line.indexOf(' ',line.indexOf(' ')+1);QString value=pos>=0?line.mid(pos+1):QString();
                            if(auto *edit=qobject_cast<QLineEdit*>(w)){
                                if(edit->echoMode()!=QLineEdit::Normal){send("Secret fields cannot be set remotely");continue;}
                                edit->setText(value);edit->setModified(true);
                            }else if(auto *edit=qobject_cast<QPlainTextEdit*>(w)){
                                if(edit->isReadOnly()){send("Read-only output cannot be changed");continue;}
                                edit->setPlainText(value.replace(QString(QChar(92)) + QChar(110), QString(QChar(10))));
                            }else if(auto *spin=qobject_cast<QSpinBox*>(w)){
                                bool valid=false;int nval=value.toInt(&valid);
                                if(!valid||nval<spin->minimum()||nval>spin->maximum()){send("Number outside permitted range");continue;}
                                spin->setValue(nval);
                            }else {send("Not a supported editable control");continue;}
                            log("Terminal edited GUI field (value not logged)");send("OK: field updated");continue;
                        }
                        if(parts[0].compare("SELECT",Qt::CaseInsensitive)==0){
                            auto *combo=qobject_cast<QComboBox*>(w);bool idxOk=false;int i=parts.value(2).toInt(&idxOk);
                            if(!combo||!idxOk||i<0||i>=combo->count()){send("Invalid combo index");continue;}
                            combo->setCurrentIndex(i);log("Terminal selected GUI combo item "+QString::number(i));send("OK: selection changed");continue;
                        }
                        if(parts[0].compare("CHECK",Qt::CaseInsensitive)==0){
                            auto *b=qobject_cast<QAbstractButton*>(w);
                            if(!b||!b->isCheckable()||parts.size()<3){send("Not a checkable control");continue;}
                            QString answer=parts[2].toUpper();if(answer!="YES"&&answer!="NO"){send("Usage: CHECK n YES/NO");continue;}
                            b->setChecked(answer=="YES");log("Terminal toggled GUI control");send("OK: check state updated");continue;
                        }
                    }
                    send("Unknown command. Enter HELP.");
                }
            });
        }
    });
}
}

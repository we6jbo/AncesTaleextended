#pragma once
#include <QtWidgets>
#include <functional>

// In-process, nonmodal decision panel shared by GUI and authenticated 53141 clients.
// No nested QMessageBox::exec or QDialog::exec is used by converted operations.
namespace ATEDialog {
class Session : public QObject {
public:
    explicit Session(QTabWidget* owner):QObject(owner),tabs(owner) {
        setObjectName("ATE_dialog_session_53141");
        page=new QWidget(tabs);
        auto* col=new QVBoxLayout(page);
        heading=new QLabel(page);heading->setWordWrap(true);
        detail=new QPlainTextEdit(page);detail->setReadOnly(true);
        auto* row=new QHBoxLayout;
        yes=new QPushButton("Yes - continue",page);
        no=new QPushButton("No - cancel",page);
        yes->setObjectName("ATE_dialog_yes");
        no->setObjectName("ATE_dialog_no");
        row->addWidget(yes);row->addWidget(no);
        col->addWidget(heading);col->addWidget(detail,1);col->addLayout(row);
        QObject::connect(yes,&QPushButton::clicked,this,[this]{reply(true);});
        QObject::connect(no,&QPushButton::clicked,this,[this]{reply(false);});
        QObject::connect(tabs,&QTabWidget::currentChanged,this,[this](int){
            if(pending && tabs->currentWidget()!=page)
                QTimer::singleShot(0,this,[this]{if(pending)tabs->setCurrentWidget(page);});
        });
    }
    QTabWidget* tabs;
    QWidget* page;
    QLabel* heading;
    QPlainTextEdit* detail;
    QPushButton* yes;
    QPushButton* no;
    QPointer<QWidget> previous;
    QString title, description;
    bool pending=false;
    std::function<void(bool)> callback;
    bool open(const QString& t,const QString& d,std::function<void(bool)> cb) {
        if(pending)return false; // refuse to replace unresolved approval
        previous=tabs->currentWidget();
        yes->setText("Yes - continue");no->show();
        title=t;description=d;callback=std::move(cb);pending=true;
        heading->setText("Confirmation required: "+t);
        detail->setPlainText(d);
        tabs->addTab(page,"Dialog");
        tabs->setCurrentWidget(page);
        yes->setFocus();
        return true;
    }
    bool notice(const QString& t,const QString& d) {
        if(!open(t,d,[](bool){}))return false;
        yes->setText("OK - close");no->hide();return true;
    }
    void reply(bool accepted) {
        if(!pending)return;
        pending=false;
        auto done=std::move(callback);callback={};
        int n=tabs->indexOf(page);
        if(n>=0)tabs->removeTab(n);
        if(previous && tabs->indexOf(previous)>=0)tabs->setCurrentWidget(previous);
        previous.clear();
        // Returning to the event loop makes the action asynchronous and prevents
        // a socket readyRead callback from being blocked waiting for a dialog.
        QTimer::singleShot(0,tabs,[done=std::move(done),accepted]{if(done)done(accepted);});
    }
};
inline Session* session(QTabWidget* tabs) {
    if(!tabs)return nullptr;
    auto* existing=tabs->findChild<QObject*>("ATE_dialog_session_53141",Qt::FindDirectChildrenOnly);
    if(existing)return static_cast<Session*>(existing);
    return new Session(tabs);
}
inline bool active(QTabWidget* tabs) {auto* s=session(tabs);return s && s->pending;}
inline QString title(QTabWidget* tabs){auto* s=session(tabs);return s&&s->pending?s->title:QString();}
inline QString text(QTabWidget* tabs){auto* s=session(tabs);return s&&s->pending?s->description:QString();}
inline void ask(QTabWidget* tabs,const QString& t,const QString& d,std::function<void(bool)> cb){
    auto* s=session(tabs);
    if(!s || !s->open(t,d,std::move(cb)))qWarning("ATE Dialog tab already has a pending confirmation");
}
inline void notice(QTabWidget* tabs,const QString& t,const QString& d){auto* s=session(tabs);if(s&&!s->notice(t,d))qWarning("ATE dialog already pending");}
inline bool reply(QTabWidget* tabs,bool yes){auto* s=session(tabs);if(!s||!s->pending)return false;s->reply(yes);return true;}
}

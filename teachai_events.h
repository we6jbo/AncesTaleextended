#pragma once
// Local-only event evidence. The record does not copy file contents, credentials or subprocess output.
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSet>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QUuid>

inline QString teachAiHome(){return QDir::homePath()+"/nov21-26-2026";}
inline void teachAiEvent(const QString &action,const QString &target=QString(),const QString &result=QString(),bool repeated=false){
    // Record each distinct operation once per running process unless it represents a repeated action.
    static QSet<QString> seen;
    const QString signature=action+QChar('\n')+target+QChar('\n')+result;
    if(!repeated && seen.contains(signature))return;
    if(!repeated)seen.insert(signature);
    const QString dir=teachAiHome();
    if(!QDir().mkpath(dir))return;
    QFile::setPermissions(dir,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
    QLockFile lock(dir+"/.analyze.lock");lock.setStaleLockTime(30000);
    if(!lock.tryLock(120))return;
    QFile file(dir+"/analyze.jsonl");
    if(file.open(QIODevice::WriteOnly|QIODevice::Append)){
        file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
        QJsonObject item{
            {"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"timestamp",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"app","AncesTaleextended"},
            {"pid",qint64(QCoreApplication::applicationPid())},
            {"action",action},
            {"target",target},
            {"result",result},
            {"source","qt-instrumented"}};
        file.write(QJsonDocument(item).toJson(QJsonDocument::Compact)+"\n");
        file.flush();file.close();
    }
    lock.unlock();
}

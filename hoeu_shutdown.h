#pragma once
// User-requested startup marker and safe, visible timed shutdown.
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>

class HoeuShutdown final : public QObject {
public:
    explicit HoeuShutdown(QApplication *app) : QObject(app), app_(app) {
        path_ = QStringLiteral("/home/we6jbo/Projects/AncesTaleextended/running-hoeu.running");
        marker_ = QByteArray("AncesTaleextended running; PID=")
            + QByteArray::number(QCoreApplication::applicationPid())
            + QByteArray("; started=")
            + QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toUtf8()
            + '\n';
    }

    bool start() {
        QSaveFile file(path_);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
            || file.write(marker_) != marker_.size() || !file.commit()) {
            qCritical("Cannot write running-hoeu.running; refusing startup.");
            return false;
        }
        connect(app_, &QCoreApplication::aboutToQuit, this, [this] {
            QFile f(path_);
            if (f.open(QIODevice::ReadOnly) && f.readAll() == marker_) {
                f.close();
                QFile::remove(path_);
            }
        });
        watch_ = new QTimer(this);
        watch_->setInterval(500);
        connect(watch_, &QTimer::timeout, this, [this] {
            if (!closing_ && !isOurMarkerPresent()) beginShutdown();
        });
        watch_->start();
        return true;
    }

private:
    bool isOurMarkerPresent() const {
        QFile f(path_);
        return f.open(QIODevice::ReadOnly) && f.readAll() == marker_;
    }

    void beginShutdown() {
        closing_ = true;
        watch_->stop();
        qWarning("running-hoeu.running missing or changed; starting safe shutdown.");
        // Cancel pending modal operations rather than approving publication or writes.
        tick_ = new QTimer(this);
        tick_->setInterval(1000);
        connect(tick_, &QTimer::timeout, this, [this] { advance(); });
        advance();
        tick_->start();
    }

    void advance() {
        if (auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
            if (!countedModal_ || countedModal_ != modal) {
                countedModal_ = modal;
                modalSeconds_ = 5;
                originalTitle_ = modal->windowTitle();
                originalButton_.clear();
                originalButtonLabel_.clear();
            }
            const QString suffix = QStringLiteral(" (default in %1 s)").arg(modalSeconds_);
            modal->setWindowTitle(originalTitle_ + QStringLiteral(" | Closing: ") + QString::number(modalSeconds_) + QStringLiteral(" s"));
            if (auto *message = qobject_cast<QMessageBox *>(modal)) {
                QAbstractButton *safe = message->button(QMessageBox::Cancel);
                if (!safe) safe = message->button(QMessageBox::No);
                if (!safe) safe = message->button(QMessageBox::Abort);
                if (!safe) safe = message->button(QMessageBox::Close);
                if (safe) {
                    if (originalButton_ != safe) {
                        originalButton_ = safe;
                        originalButtonLabel_ = safe->text();
                    }
                    safe->setText(originalButtonLabel_ + suffix);
                }
            }
            if (--modalSeconds_ <= 0) {
                QPointer<QDialog> tracked = modal;
                QPointer<QAbstractButton> selected = originalButton_;
                if (selected && qobject_cast<QMessageBox *>(modal)) selected->click();
                else modal->reject();
                if (tracked) tracked->setWindowTitle(originalTitle_);
                countedModal_.clear();
                originalButton_.clear();
            }
            return;
        }
        if (countedModal_) countedModal_.clear();
        if (!countdown_) {
            countdown_ = new QDialog(nullptr, Qt::Window | Qt::WindowStaysOnTopHint);
            countdown_->setAttribute(Qt::WA_DeleteOnClose, false);
            countdown_->setWindowTitle(QStringLiteral("AncesTaleextended - shutting down"));
            countdown_->setModal(false);
            auto *layout = new QVBoxLayout(countdown_);
            message_ = new QLabel(countdown_);
            message_->setAlignment(Qt::AlignCenter);
            message_->setMinimumWidth(340);
            layout->addWidget(message_);
            auto *button = new QPushButton(QStringLiteral("Close now"), countdown_);
            layout->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this] { finish(); });
            countdown_->show();
            countdown_->raise();
            seconds_ = 5;
        }
        message_->setText(QStringLiteral("Program is closing in %1 second%2.")
                          .arg(seconds_).arg(seconds_ == 1 ? QString() : QStringLiteral("s")));
        if (--seconds_ <= 0) {
            // Keep the 1-second message visible for a full tick.
            QTimer::singleShot(1000, this, [this] { finish(); });
            tick_->stop();
        }
    }

    void finish() {
        if (finished_) return;
        finished_ = true;
        if (tick_) tick_->stop();
        if (countdown_) countdown_->hide();
        // Request normal QWidget close events first; afterwards quit the Qt event loop.
        // Install a fallback BEFORE closeAllWindows(), because closeEvent can
        // enter a nested event loop while displaying another modal dialog.
        QTimer::singleShot(5000, app_, [app = app_] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
                dialog->reject(); // never automatically authorize a protected action
            app->quit();
        });
        app_->closeAllWindows();
        app_->quit();
    }

    QApplication *app_ = nullptr;
    QString path_;
    QByteArray marker_;
    bool closing_ = false;
    bool finished_ = false;
    QTimer *watch_ = nullptr;
    QTimer *tick_ = nullptr;
    QPointer<QDialog> countedModal_;
    QPointer<QAbstractButton> originalButton_;
    QString originalButtonLabel_;
    QString originalTitle_;
    int modalSeconds_ = 5;
    QDialog *countdown_ = nullptr;
    QLabel *message_ = nullptr;
    int seconds_ = 5;
};

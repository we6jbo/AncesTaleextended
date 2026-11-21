#pragma once
#include <QWidget>
#include <QString>
#include <QVector>
#include <QJsonObject>
class QTreeWidget; class QLineEdit; class QLabel; class QPlainTextEdit; class QComboBox;
struct GeoEntry {
 QString serial, name, difficulty, place, period, sources;
};
struct GeoMatch {
 GeoEntry geo; QJsonObject person; QString matchingReason;
};
class GeographyWidget : public QWidget {
 Q_OBJECT
public:
 explicit GeographyWidget(const QString &dataRoot, QWidget *parent=nullptr);
signals:
 void exportCompleted();
private:
 QString dataRoot_;
 QVector<GeoEntry> geography_;
 QVector<GeoMatch> matches_;
 QTreeWidget *list_=nullptr;
 QLineEdit *filter_=nullptr;
 QComboBox *view_=nullptr;
 QPlainTextEdit *details_=nullptr;
 QLabel *summary_=nullptr;
 void reload();
 void updateResults();
 void updateDetails();
 void exportSelected();
};

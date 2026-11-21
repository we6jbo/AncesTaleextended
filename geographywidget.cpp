#include "geographywidget.h"
#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QHash>
#include <QSet>

namespace {
// RFC 4180-style quoted fields, commas, CRLF and embedded newlines.
QVector<QStringList> parseCsv(const QString &text) {
 QVector<QStringList> rows; QStringList row; QString field; bool quoted=false;
 for (int i=0;i<text.size();++i) {
  const QChar c=text.at(i);
  if (quoted) {
   if (c=='"' && i+1<text.size() && text.at(i+1)=='"') {field+='"'; ++i;}
   else if(c=='"') quoted=false;
   else field+=c;
  } else if(c=='"' && field.isEmpty()) quoted=true;
  else if(c==',') {row<<field;field.clear();}
  else if(c=='\n' || c=='\r') {
   if(c=='\r' && i+1<text.size() && text.at(i+1)=='\n') ++i;
   row<<field; field.clear();
   if(!(row.size()==1 && row.first().isEmpty()))rows<<row;
   row.clear();
  } else field+=c;
 }
 if(!field.isEmpty() || !row.isEmpty()){row<<field;rows<<row;}
 return rows;
}
QString normalized(QString name) {
 name=name.toCaseFolded().normalized(QString::NormalizationForm_KD);
 name.remove(QRegularExpression("[\\p{Mn}\\p{Me}]"));
 name.replace(QRegularExpression("[^a-z0-9]+")," ");
 return name.simplified();
}
QString val(const QJsonObject &p,const char *key) {return p.value(QLatin1String(key)).toString().trimmed();}
QString tgId(const QJsonObject &p){
 for(const QJsonValue &v:p.value("identifiers").toArray()) {
  QString x=v.toString().trimmed();
  if(QRegularExpression("^TG[0-9]{6}$",QRegularExpression::CaseInsensitiveOption).match(x).hasMatch())return x.toUpper();
 }
 return "Unresolved (no verified TG###### identifier)";
}
QString cleanLocation(QString s){
 s.remove(QRegularExpression("\\s*\\([^)]*\\)"));
 return s.simplified();
}
// Only show precise locality matches when the county or city AND state agree.
// Never infer identity from a shared country or state alone.
bool overlapsSpecificPlace(const QString &candidate, const QString &geo) {
 QString a=cleanLocation(candidate),b=cleanLocation(geo);
 QStringList gp=b.split(',',Qt::SkipEmptyParts);
 if(gp.size()<2)return false;
 QString town=normalized(gp.at(0));
 if(town.size()<5 || town=="unknown" || town=="united states" || town=="usa")return false;
 QString state=normalized(gp.at(1));
 if(state.size()<3 || state=="united states" || state=="usa")return false;
 QString an=normalized(a);
 return an.contains(town) && an.contains(state);
}
QString displayDate(const QJsonObject &p, const char *dateKey, const char *yearKey){
 const QString d=val(p,dateKey);
 if(!d.isEmpty())return d;
 QJsonValue y=p.value(QLatin1String(yearKey));
 if(y.isDouble())return QString::number(y.toInt())+" (year only)";
 return "Unknown";
}
QString buildExport(const GeoMatch &m) {
 const auto &p=m.person;
 QString t;
 QTextStream out(&t);
 out<<"AncesTaleextended 0.1.4 | Geography research handoff\n";
 out<<"Generated (local time): "<<QDateTime::currentDateTime().toString(Qt::ISODate)<<"\n";
 out<<"Person: "<<(!val(p,"name").isEmpty()?val(p,"name"):m.geo.name)<<"\n";
 out<<"TG identifier: "<<tgId(p)<<"\n";
 out<<"Birth date: "<<displayDate(p,"birth_date","birth_year")<<"\n";
 out<<"Birth location: "<<(val(p,"birth_location").isEmpty()?"Unknown":val(p,"birth_location"))<<"\n";
 out<<"Death date: "<<displayDate(p,"death_date","death_year")<<"\n";
 out<<"Death location: "<<(val(p,"death_location").isEmpty()?"Unknown":val(p,"death_location"))<<"\n";
 out<<"\nGEOGRAPHY REFERENCE (embedded):\n";
 out<<"Reference person: "<<m.geo.name<<"\n";
 out<<"Reference serial (not a TG): "<<m.geo.serial<<"\n";
 out<<"Match classification: "<<m.matchingReason<<"\n";
 out<<"Geographic location: "<<m.geo.place<<"\n";
 out<<"Time period: "<<m.geo.period<<"\n";
 out<<"Research difficulty: "<<m.geo.difficulty<<"\n";
 out<<"Recommended sources: "<<m.geo.sources<<"\n";
 out<<"\nResearch caution: Geography recommendations and tentative location overlaps are leads only. Verify identity, vital facts, historical boundaries and source documents before linking family members.\n";
 return t;
}
}
GeographyWidget::GeographyWidget(const QString &dataRoot,QWidget *parent)
 :QWidget(parent),dataRoot_(dataRoot) {
 auto *layout=new QVBoxLayout(this);
 auto *toolbar=new QHBoxLayout;
 auto *exportButton=new QPushButton("Export Geography Research");
 toolbar->addWidget(exportButton);
 auto *refresh=new QPushButton("Reload Family Records");toolbar->addWidget(refresh);
 filter_=new QLineEdit;filter_->setPlaceholderText("Find name, TG, city, county, region, sources...");toolbar->addWidget(filter_,1);
 view_=new QComboBox;view_->addItems({"All geography entries","Linked names only","Possible place overlaps","Geography only (unlinked)"}); toolbar->addWidget(view_);
 layout->addLayout(toolbar);
 summary_=new QLabel;summary_->setWordWrap(true);layout->addWidget(summary_);
 auto *split=new QSplitter(Qt::Vertical);layout->addWidget(split,1);
 list_=new QTreeWidget;list_->setColumnCount(5);
 list_->setHeaderLabels({"Person","TG number","Reference location","Match type","Reference ID"});
 list_->setSelectionMode(QAbstractItemView::SingleSelection);
 list_->setAlternatingRowColors(true);list_->setRootIsDecorated(false);split->addWidget(list_);
 details_=new QPlainTextEdit;details_->setReadOnly(true);details_->setPlaceholderText("Select a person to view birth/death information and recommended research sources.");split->addWidget(details_);
 split->setStretchFactor(0,3);split->setStretchFactor(1,2);
 QFile resource(":/reference/geography.csv");
 if(!resource.open(QIODevice::ReadOnly)) {
  summary_->setText("ERROR: Embedded geography database not found.");return;
 }
 auto rows=parseCsv(QString::fromUtf8(resource.readAll()));
 QHash<QString,int> pos;
 // Validate six known columns, not arbitrary input mapping.
 if(rows.isEmpty() || rows.first()!=QStringList({"SerialNumber","DisplayName","Difficulty","InformationType","Information"})) {
  summary_->setText("ERROR: Embedded geography CSV headers are invalid.");return;
 }
 for(int i=1;i<rows.size();++i) {
  const auto &r=rows[i];if(r.size()!=5 || r[0].isEmpty()) continue;
  if(!pos.contains(r[0])){
   pos.insert(r[0],geography_.size());
   geography_.append({r[0],r[1],r[2],"","",""});
  }
  auto &g=geography_[pos.value(r[0])];
  if(r[3]=="Location")g.place=r[4];
  else if(r[3]=="Time period")g.period=r[4];
  else if(r[3]=="Recommended sources")g.sources=r[4];
 }
 connect(refresh,&QPushButton::clicked,this,&GeographyWidget::reload);
 connect(filter_,&QLineEdit::textChanged,this,&GeographyWidget::updateResults);
 connect(view_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&GeographyWidget::updateResults);
 connect(list_,&QTreeWidget::itemSelectionChanged,this,&GeographyWidget::updateDetails);
 connect(exportButton,&QPushButton::clicked,this,&GeographyWidget::exportSelected);
 reload();
}
void GeographyWidget::reload(){
 matches_.clear();QJsonArray people;
 QFile file(dataRoot_+"/missing_family_records.json");
 if(file.open(QIODevice::ReadOnly)){
  auto j=QJsonDocument::fromJson(file.readAll());people=j.object().value("people").toArray();
 }
 QHash<QString,QVector<QJsonObject>> byName;
 for(const auto &v:people){auto p=v.toObject();byName[normalized(val(p,"name"))].append(p);}
 for(const auto &g:geography_){
  QSet<QString> seen;
  for(const auto &p:byName.value(normalized(g.name))){
   QString k=val(p,"name")+"|"+tgId(p);
   seen.insert(k);
   matches_.append({g,p,"Same normalized name (identity still requires source verification)"});
  }
  // Geographic overlap is a lead, never proof that the CSV individual is the same.
  for(const auto &v:people){
   auto p=v.toObject();QString k=val(p,"name")+"|"+tgId(p);
   if(seen.contains(k))continue;
   if(overlapsSpecificPlace(val(p,"birth_location"),g.place) || overlapsSpecificPlace(val(p,"death_location"),g.place)){
    matches_.append({g,p,"Possible location overlap only (different person; NOT an identity match)"});
   }
  }
  if(!byName.contains(normalized(g.name))) matches_.append({g,{},"Geography reference only; no name match in local family JSON"});
 }
 updateResults();
}
void GeographyWidget::updateResults(){
 list_->clear();QString term=filter_->text().trimmed();int count=0;
 for(int i=0;i<matches_.size();++i){const GeoMatch &m=matches_.at(i);
  const QString person=val(m.person,"name").isEmpty()?m.geo.name:val(m.person,"name");
  const bool linked=m.matchingReason.startsWith("Same normalized");
  const bool place=m.matchingReason.startsWith("Possible location");
  const int v=view_->currentIndex();
  if(v==1&&!linked || v==2&&!place || v==3&&(linked||place))continue;
  const QString fields=person+" "+tgId(m.person)+" "+m.geo.name+" "+m.geo.place+" "+m.geo.period+" "+m.geo.sources;
  if(!term.isEmpty()&&!fields.contains(term,Qt::CaseInsensitive))continue;
  auto *item=new QTreeWidgetItem(list_,{person,tgId(m.person),m.geo.place,linked?"Name match":place?"Place lead":"Reference only",m.geo.serial});
  item->setData(0,Qt::UserRole,i);
  if(place)item->setForeground(3,QBrush(QColor(156,90,0)));
  ++count;
 }
 summary_->setText(QString("%1 embedded geography reference people; %2 local family records; %3 matching or reference rows displayed. No records sent online. ")
 .arg(geography_.size()).arg([&]{QSet<QString> names;for(const auto &m:matches_)if(!m.person.isEmpty())names.insert(val(m.person,"name"));return names.size();}()).arg(count));
 if(list_->topLevelItemCount()>0)list_->setCurrentItem(list_->topLevelItem(0));
 else details_->setPlainText("No matching geography records for this filter.");
 for(int c=0;c<list_->columnCount();++c)list_->resizeColumnToContents(c);
}
void GeographyWidget::updateDetails(){
 auto *item=list_->currentItem();if(!item){details_->clear();return;}
 const int idx=item->data(0,Qt::UserRole).toInt();
 if(idx<0||idx>=matches_.size())return;
 details_->setPlainText(buildExport(matches_.at(idx)));
}
void GeographyWidget::exportSelected(){
 auto *item=list_->currentItem();
 if(!item){QMessageBox::warning(this,"Export Geography Research","Select a person first.");return;}
 const int idx=item->data(0,Qt::UserRole).toInt();
 if(idx<0||idx>=matches_.size())return;
 const QString dir=QDir::homePath()+"/geography";
 const QString path=dir+"/share-to-chatgpt.txt";
 if(!QDir().mkpath(dir)){QMessageBox::critical(this,"Export failed","Cannot create geography directory.");return;}
 // The requested path is fixed. Never follow an existing symlink to another location.
 QFileInfo target(path);
 if(target.isSymLink()){QMessageBox::critical(this,"Export blocked","Output file is a symbolic link.");return;}
 if(target.exists() && QMessageBox::question(this,"Replace previous export","Replace the existing share-to-chatgpt.txt with the selected person's information?")!=QMessageBox::Yes)return;
 QSaveFile file(path);
 file.setDirectWriteFallback(false);
 if(!file.open(QIODevice::WriteOnly|QIODevice::Text)){
  QMessageBox::critical(this,"Export failed",file.errorString());return;
 }
 const QByteArray bytes=buildExport(matches_.at(idx)).toUtf8();
 if(file.write(bytes)!=bytes.size()||!file.commit()){
  QMessageBox::critical(this,"Export failed",file.errorString());return;
 }
 QFile::setPermissions(path,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
 emit exportCompleted();
 QMessageBox::information(this,"Export successful","Saved locally:\n"+path+"\n\nNothing was uploaded to ChatGPT.");
}

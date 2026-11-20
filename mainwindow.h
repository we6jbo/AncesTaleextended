#pragma once
#include <QMainWindow>
namespace Ui { class MainWindow; }
class MainWindow : public QMainWindow
{
    Q_OBJECT
public: explicit MainWindow(QWidget *parent=nullptr); ~MainWindow();
protected: void closeEvent(QCloseEvent *event) override;
private: Ui::MainWindow *ui;
};

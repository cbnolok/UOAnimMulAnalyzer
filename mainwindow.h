#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_spinBox_animfilenum_valueChanged(int arg1);
    void on_spinBox_idxindex_valueChanged(int arg1);
    void on_spinBox_framenumber_valueChanged(int arg1);

    void on_pushButton_browse_clicked();

private:
    Ui::MainWindow *ui;
    int m_animfilenum   = 1;
    int m_idxindex      = 0;
    int m_framenum      = 0;

    void resetView();
    void updateView();
};

#endif // MAINWINDOW_H

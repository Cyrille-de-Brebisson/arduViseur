#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMouseEvent>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
};

extern bool mouseDown;
extern int mouseX, mouseY;

class Widget : public QWidget
{
    Q_OBJECT
public:
    Widget() { startTimer(100); }
protected:
    void paintEvent(QPaintEvent *event);
    void timerEvent(QTimerEvent *event) { (void) event; update(); };
    void mousePressEvent(QMouseEvent *e)
    {
        if (e->button() == Qt::LeftButton) { mouseDown= true; mouseX= e->pos().x()+5; mouseY= e->pos().y()+10; }
    }
    void mouseMoveEvent(QMouseEvent *e)
    {
        if(mouseDown) { mouseX= e->pos().x(); mouseY= e->pos().y()+10; }
    }
    void mouseReleaseEvent(QMouseEvent *e) { mouseDown= false; }
};
#endif // MAINWINDOW_H

#include "mainwindow.h"
#include <QtWidgets/QApplication>
#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow w;
    w.resize(1000,w.height());
    QList<QScreen *> screens = QGuiApplication::screens();
    QRect screenGeometry = screens.at(0)->geometry();
    int x = (screenGeometry.width() - w.width()) / 2;
    int y = (screenGeometry.height() - w.height()) / 2;
    w.move(x, y);

    w.show();

    return a.exec();
}

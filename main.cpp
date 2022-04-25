#include "newsnakegameserver.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NewSnakeGameServer w;
    w.show();
    return a.exec();
}

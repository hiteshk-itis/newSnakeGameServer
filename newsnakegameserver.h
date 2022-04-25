#ifndef NEWSNAKEGAMESERVER_H
#define NEWSNAKEGAMESERVER_H

#include <QTcpServer>
#include <QWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class NewSnakeGameServer; }
QT_END_NAMESPACE

class NewSnakeGameServer : public QWidget
{
    Q_OBJECT
enum class SendTo{
    None,
    All,
    OnlyThisOne
};
enum class MessageType{
    JSON,
    QString
};
public:
    NewSnakeGameServer(QWidget *parent = nullptr);
    ~NewSnakeGameServer();
    void newConnection();
    void removeConnection();

    void readyRead(QTcpSocket*);
    void newMessage(QString socketRoom, QTcpSocket*, QVariant, MessageType, SendTo);
private:

    Ui::NewSnakeGameServer *ui;
    QTcpServer* d_server;
    QVector<QTcpSocket*> d_clients;

    QHash<QString, QJsonObject> d_rooms;
    QHash<QString, QSet<QTcpSocket*>> d_roomsAndClients;

    QHash<QTcpSocket*, QByteArray> d_receivedData;
};
#endif // NEWSNAKEGAMESERVER_H

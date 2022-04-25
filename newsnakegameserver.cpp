#include "newsnakegameserver.h"
#include "ui_newsnakegameserver.h"



NewSnakeGameServer::NewSnakeGameServer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::NewSnakeGameServer)
    , d_server(new QTcpServer(this))
{
    ui->setupUi(this);

    if(!d_server->listen(QHostAddress::LocalHost, 52693)){
        ui->log->setPlainText(tr("Failure while starting Server: %1").arg(d_server->errorString()));
        return;
    }

    ui->address->setText(d_server->serverAddress().toString());
    ui->port->setText(QString::number(d_server->serverPort()));

    connect(d_server, &QTcpServer::newConnection, this,         &NewSnakeGameServer::newConnection);

}

NewSnakeGameServer::~NewSnakeGameServer()
{
    delete ui;
}

void NewSnakeGameServer::newConnection()
{

    // run the loop as long as there are pending Connections
    while (d_server->hasPendingConnections()) {

        // a socket for the next connection
        QTcpSocket* socket = d_server->nextPendingConnection();

        // insert the new socket in the vector d_clients
        d_clients << socket;

        // enable the disconnect button
        ui->disconnect->setEnabled(true);

        // connect the disconnected and connected signals
        connect(socket, &QTcpSocket::disconnected,
                this, &NewSnakeGameServer::removeConnection);
        connect(socket, &QTcpSocket::readyRead, [=]{
            readyRead(socket);
        });

        ui->log->appendPlainText(tr("* New Connection: %1(%3): %2\n")
                                 .arg(socket->peerAddress().toString()).arg(QString::number(socket->peerPort())).arg(socket->peerName()));
    }
}

void NewSnakeGameServer::removeConnection()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

    if(!socket){
        return;
    }

    ui->log->appendPlainText(tr("* Connection Removed: %1, port %2\n").arg(socket->peerAddress().toString()).arg(QString::number(socket->peerPort())));

    d_clients.removeOne(socket);
    d_receivedData.remove(socket);
    socket->deleteLater();
    ui->disconnect->setEnabled(!d_clients.isEmpty());
}

void NewSnakeGameServer::readyRead(QTcpSocket* socket)
{

    // check if the socket is empty
    if(!socket){
        ui->log->appendPlainText("Error: Socket is empty");
        return;
    }
    QString socketRoom;
    QVariant v;

    for(const QString &abc: d_roomsAndClients.keys()){
        if(d_roomsAndClients.value(abc).contains(socket)){
            socketRoom = abc;
            break;
        }
    }


    // insert the socket and the corresponding incoming message
    QByteArray &buffer = d_receivedData[socket];
    buffer.append(socket->readAll());

    while (true) {
        // get the index at which one message stops
        int endIndex = buffer.indexOf(23);
        if(endIndex < 0){
            return;
        }
        QString message = QString::fromUtf8(buffer.left(endIndex));

        QJsonDocument doc = QJsonDocument::fromJson(buffer.left(endIndex));


        buffer.remove(0, endIndex+1);

        if(!doc.isNull()){
            QJsonObject d_roomInfo = doc.object();
            QJsonValue purpose = d_roomInfo.value(tr("purpose"));


            if((purpose.toString() == "set Initial Position") || (purpose.toString() == "set Direction")){
                v.setValue(doc);
                newMessage(socketRoom,
                           socket,
                           v,
                           MessageType::JSON,
                           SendTo::All);

                break;
            }

            QJsonValue roomName = d_roomInfo.value(tr("roomName"));

            if(roomName.toString().isEmpty()){
                ui->log->appendPlainText("Error: room Name empty while create room");
                v.setValue(tr("Error: room Name empty while create room"));
                newMessage(socketRoom, socket, v, MessageType::QString, SendTo::OnlyThisOne);
                return;
            }

            if(purpose.toString() == "Create Room"){
                // save the room info
                QJsonObject &obj = d_rooms[roomName.toString()];
                obj = d_roomInfo;

            }
            if(!d_rooms.contains(roomName.toString())){
                ui->log->appendPlainText(tr("Error: no room as %1 for join room").arg(roomName.toString()));

                v.setValue(tr("Error: no room as %1 for join room").arg(roomName.toString()));
                newMessage(socketRoom, socket, v, MessageType::QString, SendTo::OnlyThisOne);
                return;
            }
            // save the rooms and clients info
            QSet<QTcpSocket*> &participants = d_roomsAndClients[roomName.toString()];
            QJsonObject obj = d_rooms[roomName.toString()];
            QJsonValue size = obj.value(tr("players"));

            if(participants.size() != size.toInt()){
                participants << socket;
                v.setValue(tr("Success: %1/%2 have participated in room %3.").arg(participants.size()).arg(size.toInt()).arg(roomName.toString()));
                SendTo s = (purpose.toString() == "Create Room") ? (SendTo::OnlyThisOne) : SendTo::All;

                newMessage(roomName.toString(),
                           socket,
                           v,
                           MessageType::QString,
                           s);


            }
            else{
                ui->log->appendPlainText(tr("Error: room %1 has exceeded %2 participants").arg(roomName.toString()).arg(size.toInt()));
                v.setValue(tr("Error: room %1 has exceeded %2 participants").arg(roomName.toString()).arg(size.toInt()));
                newMessage(socketRoom, socket, v, MessageType::QString, SendTo::OnlyThisOne);

                return;

            }
            if(participants.size() == size.toInt()){
                QJsonObject obj;
                obj.insert("purpose", "All Players Joined");
                obj.insert("players", participants.size());
                QJsonDocument doc(obj);
                v.setValue(doc);
                newMessage(roomName.toString(),
                           socket,
                           v,
                           MessageType::JSON,
                           SendTo::All);
                break;
            }else{
                break;
            }

            v.setValue("Success: "+purpose.toString()+" done");
            newMessage(socketRoom, socket, v, MessageType::QString, SendTo::OnlyThisOne);

            return;
        }
        v.setValue(message);
        newMessage(socketRoom, socket, v,MessageType::QString, SendTo::All);
    }
}

void NewSnakeGameServer::newMessage(QString socketRoom, QTcpSocket* sender, QVariant v, MessageType m, SendTo s)
{
    // purpose: send the incoming message to all the sockets in the d_clients
    QByteArray msgArray;
    if(m == MessageType::JSON){
        QJsonDocument doc = v.toJsonDocument();
        msgArray = doc.toJson(QJsonDocument::Indented);

        ui->log->appendPlainText("Sending Message: " + doc.toJson() + "\n");
    }
    else{
        // -- pack the message before sending it
        // change the message to byte array
        QString message = v.toString();
        msgArray = message.toUtf8();
        ui->log->appendPlainText(tr("Sending Message %1\n").arg(message));
    }



    // append the end of line
    msgArray.append(23);

    // write to all the sockets in clients
    if(s == SendTo::All){
        for(QTcpSocket* socket: d_roomsAndClients.value(socketRoom)){
            if((socket->state() == QAbstractSocket::ConnectedState)){
                socket->write(msgArray);
            }
        }
    }
    else if(s == SendTo::OnlyThisOne){
        if(sender->state() == QAbstractSocket::ConnectedState){
            sender->write(msgArray);
        }
    }

}

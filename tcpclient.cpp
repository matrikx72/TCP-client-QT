#include "tcpclient.h"

/*
 * Перегрузка операторов для сериализации заголовка.
 * Это нужно, чтобы отправлять и принимать структуру через QDataStream.
 */
QDataStream &operator >>(QDataStream &out, ServiceHeader &data){
    out >> data.id;
    out >> data.idData;
    out >> data.status;
    out >> data.len;
    return out;
};

QDataStream &operator <<(QDataStream &in, ServiceHeader &data){
    in << data.id;
    in << data.idData;
    in << data.status;
    in << data.len;
    return in;
};

/*!
 * \brief Конструктор класса TCPclient
 * Здесь мы создаем сокет и подключаем его сигналы к нашим слотам.
 */
TCPclient::TCPclient(QObject *parent) : QObject(parent)
{
    socket = new QTcpSocket(this);

    // Когда данные готовы к чтению, вызываем наш слот
    connect(socket, &QTcpSocket::readyRead, this, &TCPclient::ReadyReed);

    // Когда соединение установлено, отправляем сигнал об успехе (статус 1)
    connect(socket, &QTcpSocket::connected, this, [this](){
        emit sig_connectStatus(STATUS_SUCCES);
    });

    // Когда соединение разорвано, отправляем сигнал
    connect(socket, &QTcpSocket::disconnected, this, &TCPclient::sig_Disconnected);

    // Обработка ошибок сокета
    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError){
        emit sig_connectStatus(ERR_CONNECT_TO_HOST);
    });
}

/*!
 * \brief Метод отправляет запрос на сервер (только заголовок)
 */
void TCPclient::SendRequest(ServiceHeader head)
{
    QByteArray data;
    QDataStream outStr(&data, QIODevice::WriteOnly);
    outStr.setByteOrder(QDataStream::BigEndian);

    outStr << head;

    socket->write(data);
    socket->flush();
}

/*!
 * \brief Метод отправляет данные (заголовок + строка) на сервер
 */
void TCPclient::SendData(ServiceHeader head, QString str)
{
    QByteArray data;
    QDataStream outStr(&data, QIODevice::WriteOnly);
    outStr.setByteOrder(QDataStream::BigEndian);
    outStr << head;
    outStr << str;

    socket->write(data);
    socket->flush();
}

/*!
 * \brief Метод подключения к серверу
 */
void TCPclient::ConnectToHost(QHostAddress host, uint16_t port)
{
    socket->connectToHost(host, port);
}

/*!
 * \brief Метод отключения от сервера
 */
void TCPclient::DisconnectFromHost()
{
    socket->disconnectFromHost();
}

/*!
 * \brief Слот для чтения данных из сокета
 * Здесь мы читаем заголовок и, если он валиден, вызываем ProcessingData
 */
void TCPclient::ReadyReed()
{
    QDataStream incStream(socket);
    incStream.setByteOrder(QDataStream::BigEndian);

    if(incStream.status() != QDataStream::Ok){
        QMessageBox msg;
        msg.setIcon(QMessageBox::Warning);
        msg.setText("Ошибка открытия входящего потока для чтения данных!");
        msg.exec();
        return;
    }

    // Читаем до конца потока
    while(incStream.atEnd() == false){
        if(servHeader.idData == 0){
            // Проверяем, достаточно ли байт для чтения заголовка
            if(socket->bytesAvailable() < sizeof(ServiceHeader)){
                return;
            }
            else{
                // Читаем заголовок
                incStream >> servHeader;

                // Проверяем ID начала сообщения
                if(servHeader.id != ID){
                    // Если ID не совпадает, ищем его в потоке
                    uint16_t hdr = 0;
                    while(!incStream.atEnd()){
                        incStream >> hdr;
                        if(hdr == ID){
                            incStream >> servHeader.idData;
                            incStream >> servHeader.status;
                            incStream >> servHeader.len;
                            break;
                        }
                    }
                }
            }
        }

        // Если данных меньше, чем указано в заголовке, ждем
        if(socket->bytesAvailable() < servHeader.len){
            return;
        }
        else{
            // Обрабатываем данные
            ProcessingData(servHeader, incStream);
            // Сбрасываем заголовок для следующего пакета
            servHeader.idData = 0;
            servHeader.status = 0;
            servHeader.len = 0;
        }
    }
}

/*!
 * \brief Обработка полученных данных в зависимости от типа
 */
void TCPclient::ProcessingData(ServiceHeader header, QDataStream &stream)
{
    switch (header.idData){
        case GET_TIME: {
            QDateTime time;
            stream >> time;
            emit sig_sendTime(time);
            break;
        }
        case GET_SIZE: {
            uint32_t size;
            stream >> size;
            emit sig_sendFreeSize(size);
            break;
        }
        case GET_STAT: {
            StatServer stat;
            stream >> stat.incBytes;
            stream >> stat.sendBytes;
            stream >> stat.revPck;
            stream >> stat.sendPck;
            stream >> stat.workTime;
            stream >> stat.clients;
            emit sig_sendStat(stat);
            break;
        }
       case SET_DATA: {
                if (header.status == STATUS_SUCCES) {
                    QString reply;
                    stream >> reply;
                    emit sig_SendReplyForSetData(reply);
                } else {
                    emit sig_Error(header.status);
                }
                break;
            }
        case CLEAR_DATA: {
            // При очистке сервер присылает только заголовок со статусом
            if (header.status == STATUS_SUCCES) {
                emit sig_Success(CLEAR_DATA);
            } else {
                emit sig_Error(header.status);
            }
            break;
        }
        default:
            break;
    }
}

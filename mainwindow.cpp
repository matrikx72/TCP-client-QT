#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    client = new TCPclient(this);

    // Доступность полей по умолчанию
    ui->le_data->setEnabled(false);
    ui->pb_request->setEnabled(false);
    ui->lb_connectStatus->setText("Отключено");
    ui->lb_connectStatus->setStyleSheet("color: red");
    ui->spB_port->setValue(12345); // Установим порт по умолчанию

    // Подключаем сигналы от клиента к нашим слотам
    connect(client, &TCPclient::sig_connectStatus, this, &MainWindow::DisplayConnectStatus);
    connect(client, &TCPclient::sig_Disconnected, this, [&]{
        // При отключении меняем надписи и доступность полей.
        ui->lb_connectStatus->setText("Отключено");
        ui->lb_connectStatus->setStyleSheet("color: red");
        ui->pb_connect->setText("Подключиться");
        ui->le_data->setEnabled(false);
        ui->pb_request->setEnabled(false);
        ui->spB_port->setEnabled(true);
        ui->spB_ip1->setEnabled(true);
        ui->spB_ip2->setEnabled(true);
        ui->spB_ip3->setEnabled(true);
        ui->spB_ip4->setEnabled(true);
    });

    connect(client, &TCPclient::sig_sendTime, this, &MainWindow::DisplayTime);
    connect(client, &TCPclient::sig_sendFreeSize, this, &MainWindow::DisplayFreeSpace);
    connect(client, &TCPclient::sig_sendStat, this, &MainWindow::DisplayStat);
    connect(client, &TCPclient::sig_SendReplyForSetData, this, &MainWindow::SetDataReply);
    connect(client, &TCPclient::sig_Error, this, &MainWindow::DisplayError);
    connect(client, &TCPclient::sig_Success, this, &MainWindow::DisplaySuccess);

    // Обработка изменения индекса запроса
    connect(ui->cb_request, &QComboBox::currentIndexChanged, this, &MainWindow::on_cb_request_currentIndexChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

/*!
 * \brief Отображает текущее время от сервера
 */
void MainWindow::DisplayTime(QDateTime time)
{
    ui->tb_result->append("Текущие время и дата на сервере: " + time.toString("dd MMM yyyy hh:mm:ss"));
}

/*!
 * \brief Отображает свободное место на сервере
 */
void MainWindow::DisplayFreeSpace(uint32_t freeSpace)
{
    ui->tb_result->append("Свободное место на сервере: " + QString::number(freeSpace) + " байт");
}

/*!
 * \brief Отображает квитанцию об успешной отправке данных
 */
void MainWindow::SetDataReply(QString replyString)
{
    ui->tb_result->append("Ответ сервера на данные: " + replyString);
}

/*!
 * \brief Отображает статистику сервера
 */
void MainWindow::DisplayStat(StatServer stat)
{
    ui->tb_result->append("Статистика сервера:");
    ui->tb_result->append("  Принято байт: " + QString::number(stat.incBytes));
    ui->tb_result->append("  Передано байт: " + QString::number(stat.sendBytes));
    ui->tb_result->append("  Принято пакетов: " + QString::number(stat.revPck));
    ui->tb_result->append("  Передано пакетов: " + QString::number(stat.sendPck));
    ui->tb_result->append("  Время работы (сек): " + QString::number(stat.workTime));
    ui->tb_result->append("  Подключено клиентов: " + QString::number(stat.clients));
}

/*!
 * \brief Отображает ошибки
 */
void MainWindow::DisplayError(uint16_t error)
{
    switch (error) {
    case ERR_NO_FREE_SPACE:
        ui->tb_result->append("Ошибка: Недостаточно свободного места на сервере.");
        break;
    case ERR_CONNECT_TO_HOST:
        ui->tb_result->append("Ошибка: Не удалось подключиться к серверу.");
        break;
    case ERR_NO_FUNCT:
        ui->tb_result->append("Ошибка: Функционал не реализован.");
        break;
    default:
        ui->tb_result->append("Неизвестная ошибка: " + QString::number(error));
        break;
    }
}

/*!
 * \brief Отображает успешное выполнение запроса
 */
void MainWindow::DisplaySuccess(uint16_t typeMess)
{
    switch (typeMess) {
    case CLEAR_DATA:
        ui->tb_result->append("Память сервера успешно очищена!");
        break;
    default:
        break;
    }
}

/*!
 * \brief Отображает статус подключения
 */
void MainWindow::DisplayConnectStatus(uint16_t status)
{
    if(status == ERR_CONNECT_TO_HOST){
        ui->tb_result->append("Ошибка подключения к порту: " + QString::number(ui->spB_port->value()));
    }
    else{
        ui->lb_connectStatus->setText("Подключено");
        ui->lb_connectStatus->setStyleSheet("color: green");
        ui->pb_connect->setText("Отключиться");
        ui->spB_port->setEnabled(false);
        ui->pb_request->setEnabled(true);
        ui->spB_ip1->setEnabled(false);
        ui->spB_ip2->setEnabled(false);
        ui->spB_ip3->setEnabled(false);
        ui->spB_ip4->setEnabled(false);
    }
}

/*!
 * \brief Обработчик кнопки подключения/отключения
 */
void MainWindow::on_pb_connect_clicked()
{
    if(ui->pb_connect->text() == "Подключиться"){
        uint16_t port = ui->spB_port->value();
        QString ip = ui->spB_ip4->text() + "." +
                     ui->spB_ip3->text() + "." +
                     ui->spB_ip2->text() + "." +
                     ui->spB_ip1->text();
        client->ConnectToHost(QHostAddress(ip), port);
    }
    else{
        client->DisconnectFromHost();
    }
}

/*!
 * \brief Обработчик кнопки "Отправить"
 */
void MainWindow::on_pb_request_clicked()
{
    ServiceHeader header;
    header.id = ID;
    header.status = STATUS_SUCCES;
    header.len = 0;

    switch (ui->cb_request->currentIndex()){
        case 0: // Получить время
            header.idData = GET_TIME;
            client->SendRequest(header);
            break;
        case 1: // Получить свободное место
            header.idData = GET_SIZE;
            client->SendRequest(header);
            break;
        case 2: // Получить статистику
            header.idData = GET_STAT;
            client->SendRequest(header);
            break;
        case 3: // Отправить данные
            header.idData = SET_DATA;
           {
               QString data = ui->le_data->text();
               if (data.isEmpty()) {
                   ui->tb_result->append("Ошибка: Введите текст для отправки.");
                   return;
               }
               header.len = data.toUtf8().size();
               client->SendData(header, data);
           }
           break;
        case 4: // Очистить память на сервере
            header.idData = CLEAR_DATA;
            client->SendRequest(header);
            break;
        default:
            ui->tb_result->append("Такой запрос не реализован.");
            return;
    }
}

/*!
 * \brief Обработчик изменения индекса запроса
 */
void MainWindow::on_cb_request_currentIndexChanged(int index)
{
    // Разблокируем поле отправления данных только когда выбрано "Отправить данные"
    if(ui->cb_request->currentIndex() == 3){
        ui->le_data->setEnabled(true);
    }
    else{
        ui->le_data->setEnabled(false);
    }
}

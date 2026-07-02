#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QSerialPortInfo>
#include <QFileDialog>
#include <QSettings>
#include <QDebug>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QMessageBox>
#include <QCloseEvent>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isConnected(false)
    , m_isMeasuring(false)
    , m_currentMode(0)
    , m_currentUnit(0)
    , m_batteryLevel(0)
    , m_logFile(nullptr)
    , m_logStream(nullptr)
{
    ui->setupUi(this);
    this->showMaximized();
    // 初始化UI
    initUI();

    // 初始化串口
    initSerialPort();

    // 初始化曲线
    initPlot();

    // 初始化定时器
    m_connectTimer = new QTimer(this);
    m_connectTimer->setInterval(1000);  // 1秒

    m_plotUpdateTimer = new QTimer(this);
    m_plotUpdateTimer->setInterval(100); // 100ms

    // 串口自动刷新定时器（每秒扫描一次）
    m_serialRefreshTimer = new QTimer(this);
    m_serialRefreshTimer->setInterval(1000);
    connect(m_serialRefreshTimer, &QTimer::timeout, this, &MainWindow::updateSerialPortList);
    m_serialRefreshTimer->start();

    // 初始化连接
    initConnections();

    // 更新串口列表
    updateSerialPortList();

    // 加载配置
    loadConfig();

    // 添加启动日志
    addToLog(QStringLiteral("程序启动"), false);
    addToLog(QStringLiteral("波特率: 115200, 数据位: 8, 停止位: 1, 校验位: 无"), false);

    updateModeIcon(0);
    updateBrightnessIcon(1);
    updateMotorIcon(true);
    updateBatteryIcon(4);
    updateUnitIcons(0);
}

MainWindow::~MainWindow()
{
    // 关闭串口
    if(m_serial.isOpen()) {
        m_serial.close();
    }

    // 保存配置
    saveConfig();

    // 关闭日志文件
    if(m_logStream) {
        delete m_logStream;
    }
    if(m_logFile) {
        m_logFile->close();
        delete m_logFile;
    }

    delete ui;
}

void MainWindow::initUI()
{
    // 设置窗口标题
    setWindowTitle(QStringLiteral("SNDWAY SW-3000A&B电脑软件"));

    // 设置状态栏
    ui->statusbar->showMessage(QStringLiteral("就绪"));

    // 设置表格列宽
    ui->tableWidget->setColumnWidth(0, 180);  // 时间
    ui->tableWidget->setColumnWidth(1, 140);  // 主显示参数
    ui->tableWidget->setColumnWidth(2, 140);   // 辅显参数I
    ui->tableWidget->setColumnWidth(3, 140);   // 辅显参数II
    ui->tableWidget->setColumnWidth(4, 155);   // 模式

    // 设置表格可排序
    ui->tableWidget->setSortingEnabled(false);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 设置日志文本框只读
    //ui->plainTextEdit_log->setReadOnly(true);

    // 设置默认波特率
    ui->comboBox_baud->setCurrentText("115200");

    // 设置默认模式
    ui->comboBox_mode->setCurrentIndex(0);

    // ========== 加深 DIS0/DIS1/DIS2 边框 ==========
    ui->DIS0->setStyleSheet("border: 2px solid #2D3748; border-radius: 4px; padding: 4px;");
    ui->DIS1->setStyleSheet("border: 2px solid #2D3748; border-radius: 4px; padding: 4px;");
    ui->DIS2->setStyleSheet("border: 2px solid #2D3748; border-radius: 4px; padding: 4px;");

    ui->DIS0->setAlignment(Qt::AlignCenter);
    ui->DIS1->setAlignment(Qt::AlignCenter);
    ui->DIS2->setAlignment(Qt::AlignCenter);

    ui->action_save->setEnabled(false);
}

void MainWindow::initSerialPort()
{
    // 连接串口信号
    connect(&m_serial, &QSerialPort::readyRead, this, &MainWindow::slot_onSerialDataReceived);
    connect(&m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if(error != QSerialPort::NoError) {
            addToLog(QStringLiteral("串口错误: %1").arg(m_serial.errorString()), false);
            ui->statusbar->showMessage(QStringLiteral("串口错误: ") + m_serial.errorString());
        }
    });
}

void MainWindow::initPlot()
{
    // 获取曲线控件（假设是QWidget，实际使用时需要转换为QCustomPlot）
    QCustomPlot *plot = qobject_cast<QCustomPlot*>(ui->plotWidget);
    if(!plot) {
        // 如果ui->plotWidget不是QCustomPlot，需要替换
        // 这里简单起见，假设已经正确设置了
        return;
    }


    // 设置 x 轴范围从 0 开始
   //plot->xAxis->setRange(0, 1000);  // 范围: 0 到 100

   // 设置 y 轴范围从 0 开始
   plot->yAxis->setRange(0, 5000);  // 范围: 0 到 100
    // 设置曲线
    plot->addGraph();  // 距离曲线
    plot->graph(0)->setPen(QPen(Qt::blue));
    plot->graph(0)->setName(QStringLiteral("主显示参数"));

    plot->addGraph();  // 高度曲线
    plot->graph(1)->setPen(QPen(Qt::red));
    plot->graph(1)->setName(QStringLiteral("辅显参数I"));

    plot->addGraph();  // 水平距离曲线
    plot->graph(2)->setPen(QPen(Qt::green));
    plot->graph(2)->setName(QStringLiteral("辅显参数II"));

    // 设置坐标轴
    plot->xAxis->setLabel(QStringLiteral("时间 "));
    plot->yAxis->setLabel(QStringLiteral("距离 (米)"));

    // 设置图例
    plot->legend->setVisible(true);
    plot->legend->setFont(QFont("宋体", 9));

    // 设置网格
    plot->xAxis->grid()->setVisible(true);
    plot->yAxis->grid()->setVisible(true);

    // 设置范围
    plot->xAxis->setRange(0, 60);
    plot->yAxis->setRange(0, 100);

    // 允许用户交互
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void MainWindow::initConnections()
{
    // 定时器
    connect(m_connectTimer, &QTimer::timeout, this, &MainWindow::onConnectTimer);
    connect(m_plotUpdateTimer, &QTimer::timeout, this, &MainWindow::onPlotUpdateTimer);

    // 串口
    connect(&m_serial, &QSerialPort::readyRead, this, &MainWindow::slot_onSerialDataReceived);
    connect(&m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if(error != QSerialPort::NoError) {
            addToLog(QStringLiteral("串口错误: %1").arg(m_serial.errorString()), false);
            ui->statusbar->showMessage(QStringLiteral("串口错误: ") + m_serial.errorString());
        }
    });
}

void MainWindow::updateSerialPortList()
{
    QString currentSel = ui->comboBox_SerialPortInfo->currentText();
    ui->comboBox_SerialPortInfo->clear();
    bool hasPort = false;

    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QString portName = info.portName();
        ui->comboBox_SerialPortInfo->addItem(portName);
        if (portName == currentSel) hasPort = true;
    }

    if (ui->comboBox_SerialPortInfo->count() == 0) {
        ui->comboBox_SerialPortInfo->addItem(QStringLiteral("无可用串口"));
    } else {
        if (hasPort && !currentSel.isEmpty() && currentSel != "无可用串口") {
            ui->comboBox_SerialPortInfo->setCurrentText(currentSel);
        }
    }
}

// 打开串口
void MainWindow::on_pushButton_open_clicked()
{
    if(m_serial.isOpen()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("串口已打开"));
        return;
    }

    QString portName = ui->comboBox_SerialPortInfo->currentText();
    if(portName == "无可用串口") {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("没有可用串口"));
        return;
    }

    // 设置串口参数
    m_serial.setPortName(portName);
    m_serial.setBaudRate(ui->comboBox_baud->currentText().toInt());
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    if(m_serial.open(QIODevice::ReadWrite))
    {
        addToLog(QStringLiteral("串口 %1 打开成功，波特率 %2")
                 .arg(portName)
                 .arg(ui->comboBox_baud->currentText()), false);
        ui->statusbar->showMessage(QStringLiteral("串口已打开: %1").arg(portName));

        // 启动连接检测定时器
        //m_connectTimer->start();
    }
    else
    {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("打开串口失败: ") + m_serial.errorString());
        addToLog(QStringLiteral("串口打开失败: %1").arg(m_serial.errorString()), false);
    }
}

// 关闭串口
void MainWindow::on_pushButton_2_clicked()
{
    if(m_serial.isOpen()) {
        m_serial.close();
        m_connectTimer->stop();
        m_isConnected = false;
        addToLog(QStringLiteral("串口已关闭"), false);
        ui->statusbar->showMessage(QStringLiteral("串口已关闭"));
    }
}

// 接收数据
void MainWindow::slot_onSerialDataReceived()
{
    QByteArray data = m_serial.readAll();
    m_receiveBuffer.append(data);

    // 按行处理
    while(m_receiveBuffer.contains('\n'))
    {
        int index = m_receiveBuffer.indexOf('\n');
        QByteArray line = m_receiveBuffer.left(index).trimmed();
        m_receiveBuffer.remove(0, index + 1);

        if(!line.isEmpty())
        {
            QString str = QString::fromUtf8(line);
            addToLog(str, true);
            parseData(str);
        }
    }

    sendCommand("OK+Connect");
    sendCommand("OK+Distance");

}

// 发送命令
void MainWindow::sendCommand(const QString &cmd)
{
    if(!m_serial.isOpen()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先打开串口"));
        return;
    }

    QByteArray data = cmd.toUtf8();
    data.append("\r\n");  // 添加回车换行

    m_serial.write(data);
    m_serial.waitForBytesWritten(100);

    addToLog(cmd, false);
}

// 解析数据
void MainWindow::parseData(const QString &data)
{
    QString result = data;
    if (data.compare("AT+CONNECT", Qt::CaseInsensitive) == 0) {
        sendCommand("OK+Connect");   // 自动回复
        sendCommand("OK+Distance");  // 同时继续请求距离数据
        return;  // 处理完成，直接返回
    }

    if(result.contains("AT+Distance="))
    {
        result.remove("AT+Distance=");

        QStringList txtList = result.split(",");

        //int mode = ui->comboBox_mode->currentIndex();
        // 模式0/1：只允许 1个2个数值，仅保留第一个数值 DIS0
        //if (mode == 0 || mode == 1) {
        if (m_lastModeIdx == 0 || m_lastModeIdx == 1) {
            // 长度不符合要求，直接返回不处理数值 或
            if (txtList.length() < 1 || txtList.length() > 2) {
                return;
            }
            // 只保留第一个数值，丢弃第二个及以后的所有数据
            txtList = txtList.mid(0, 1);
        }
        // 模式2/3：允许3个数值 或 4个数值，仅保留前三个数值 DIS0/DIS1/DIS2
        //else if (mode == 2 || mode == 3) {
        else if (m_lastModeIdx == 2 || m_lastModeIdx == 3) {
            // 长度不符合要求，直接返回不处理
            if (txtList.length() < 3 || txtList.length() > 4) {
                return;
            }
            // 只保留前三个数值，丢弃第四个angle
            txtList = txtList.mid(0, 3);
        }
        // 其他未知模式，不处理
        else {
            return;
        }

        if(txtList.length() == 3 && txtList[0] == "0.0" && txtList[1] == "0.0" && txtList[2] == "0.0")
        {
            return;
        }

        //if(ui->comboBox_mode->currentIndex() == 3 && txtList.length() == 1)
        if(m_lastModeIdx == 3 && txtList.length() == 1)
        {
            return;
        }

        if(txtList.length() == 1)
        {
            MeasureData m;
            int row = ui->tableWidget->rowCount();
            ui->tableWidget->insertRow(row);
            // 获取当前时间
            QTime currentTime = QTime::currentTime();
            //QString unit = ui->comboBox_unit->currentIndex() == 0 ? "m" : "y";
            ui->tableWidget->setItem(row, 0, new QTableWidgetItem(currentTime.toString("hh:mm:ss.zzz")));
            //ui->tableWidget->setItem(row, 1, new QTableWidgetItem(txtList[0]+ " " + unit));
            ui->tableWidget->setItem(row, 1, new QTableWidgetItem(txtList[0]+ " " + m_lastUnit));
            //ui->tableWidget->setItem(row, 4, new QTableWidgetItem(ui->comboBox_mode->currentText()));
            ui->tableWidget->setItem(row, 4, new QTableWidgetItem(m_lastMode));
            m.distance = txtList[0].toDouble();
            m.timestamp = QDateTime::currentDateTime();  // 获取当前日期时间
            //m.mode = ui->comboBox_mode->currentIndex();
            //m.unit = ui->comboBox_unit->currentIndex();
            m.mode = m_lastModeIdx;
            m.unit = m_lastUnitIdx;
            updateDisplayGroup(m.distance, -999999, -999999);
            // 滚动到最后一行
            ui->tableWidget->scrollToBottom();
            m_dataList.append(m);
            ui->action_save->setEnabled(true);
        }
        else if(txtList.length() == 3)
        {
            int row = ui->tableWidget->rowCount();
            ui->tableWidget->insertRow(row);
            // 获取当前时间
            QTime currentTime = QTime::currentTime();
            //QString unit = ui->comboBox_unit->currentIndex() == 0 ? "m" : "y";
            ui->tableWidget->setItem(row, 0, new QTableWidgetItem(currentTime.toString("hh:mm:ss.zzz")));
            /*ui->tableWidget->setItem(row, 1, new QTableWidgetItem(txtList[0]+ " " + unit));
            ui->tableWidget->setItem(row, 2, new QTableWidgetItem(txtList[1]+ " " + unit));
            ui->tableWidget->setItem(row, 3, new QTableWidgetItem(txtList[2]+ " " + unit));*/
            ui->tableWidget->setItem(row, 1, new QTableWidgetItem(txtList[0]+ " " + m_lastUnit));
            ui->tableWidget->setItem(row, 2, new QTableWidgetItem(txtList[1]+ " " + m_lastUnit));
            ui->tableWidget->setItem(row, 3, new QTableWidgetItem(txtList[2]+ " " + m_lastUnit));
            //ui->tableWidget->setItem(row, 4, new QTableWidgetItem(ui->comboBox_mode->currentText()));
            ui->tableWidget->setItem(row, 4, new QTableWidgetItem(m_lastMode));
            MeasureData m;
            m.distance = txtList[0].toDouble();
            m.height = txtList[1].toDouble();
            m.horizontal = txtList[2].toDouble();
            m.timestamp = QDateTime::currentDateTime();  // 获取当前日期时间
            //m.mode = ui->comboBox_mode->currentIndex();
            //m.unit = ui->comboBox_unit->currentIndex();
            m.mode = m_lastModeIdx;
            m.unit = m_lastUnitIdx;
            updateDisplayGroup(m.distance, m.height, m.horizontal);
            m_dataList.append(m);
            ui->action_save->setEnabled(true);
            // 滚动到最后一行
            ui->tableWidget->scrollToBottom();
        }
    }

    addToLog(result, false);
    updatePlot();
    // 解析模式返回
    if(data == "OK+Mode")
    {
        addToLog(QStringLiteral("模式设置成功"), false);
    }

    if(data.startsWith("AT+Mode"))
    {
        QString Mode = data;
        int modeInt = Mode.remove("AT+Mode=").toInt();
        if(modeInt > 3 || modeInt < 0)
        {
           return;
        }
        ui->comboBox_mode->setCurrentIndex(modeInt);
        m_lastMode = ui->comboBox_mode->currentText();
        m_lastModeIdx = modeInt;

        updateModeIcon(modeInt);
        updateDisplayGroup(-999999, -999999, -999999);
        addToLog(QStringLiteral("模式修改成功"), false);
    }

    if(data.startsWith("AT+Unit"))
    {
        QString Unit = data;
        int modeInt = Unit.remove("AT+Unit=").toInt();
        if(modeInt > 1 || modeInt < 0)
        {
            return;

        }
        ui->comboBox_unit->setCurrentIndex(modeInt);
        m_lastUnitIdx = ui->comboBox_unit->currentIndex();
        m_lastUnit = m_lastUnitIdx == 0 ? "m" : "y";
        ui->DIS0->clear();
        ui->DIS1->clear();
        ui->DIS2->clear();

        ui->DIS0->setText("---.-");
        ui->DIS1->setText("---.-");
        ui->DIS2->setText("---.-");

        addToLog(QStringLiteral("单位修改成功"), false);
        updateUnitIcons(modeInt);
    }

    // ========== 亮度实时同步 ==========
     if(data.startsWith("AT+Brightness="))
     {
         QString strVal = data;
         int bri = strVal.remove("AT+Brightness=").toInt();
         if(bri >= 1 && bri <= 4)
         {
             ui->comboBox_brightness->setCurrentIndex(bri - 1);
             addToLog(QStringLiteral("亮度同步更新:%1").arg(bri), false);
             updateBrightnessIcon(bri);
         }
     }

     // ========== 马达实时同步 ==========
     if(data.startsWith("AT+Motor="))
     {
         QString strVal = data;
         int motorSta = strVal.remove("AT+Motor=").toInt();
         if(motorSta == 0 || motorSta == 1)
         {
             ui->comboBox_motor->setCurrentIndex(motorSta);
             addToLog(QStringLiteral("马达状态同步更新:%1")
                          .arg(motorSta ? QStringLiteral("开启") : QStringLiteral("关闭")), false);
             updateMotorIcon(motorSta == 1);
         }
     }
    // 解析连接返回
    if(data == "OK+Connect")
    {
        m_isConnected = true;
        ui->statusbar->showMessage(QStringLiteral("已连接到设备"));
        addToLog(QStringLiteral("设备连接成功"), false);
    }  //
    // 解析电池电量
    if(data.startsWith("AT+BATT="))
    {
        QStringList parts = data.split('=');
        if(parts.size() == 2) {
            m_batteryLevel = parts[1].toInt();
            updateBatteryDisplay();
            updateBatteryIcon(m_batteryLevel);
        }
    }
}

// 添加日志
void MainWindow::addToLog(const QString &text, bool isReceived)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString prefix = isReceived ? "RX << " : "TX >> ";
    QString logLine = QString("[%1] %2%3").arg(timestamp).arg(prefix).arg(text);

    //ui->plainTextEdit_log->appendPlainText(logLine);

    // 写入日志文件
    if(m_logStream) {
        *m_logStream << logLine << "\n";
        m_logStream->flush();
    }
}

// 更新表格
void MainWindow::updateTable()
{
    if(m_dataList.isEmpty()) return;

    const MeasureData &md = m_dataList.last();

    int row = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(row);

    ui->tableWidget->setItem(row, 0, new QTableWidgetItem(md.timestamp.toString("hh:mm:ss.zzz")));
    ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(md.distance, 'f', 3)));
    ui->tableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(md.height, 'f', 3)));
    ui->tableWidget->setItem(row, 3, new QTableWidgetItem(QString::number(md.horizontal, 'f', 3)));
    ui->tableWidget->setItem(row, 4, new QTableWidgetItem(QString::number(md.angle, 'f', 2)));

    // 滚动到最后一行
    ui->tableWidget->scrollToBottom();
}

// 更新曲线
void MainWindow::updatePlot()
{
    if (!m_plotUpdateTimer->isActive()) {
        m_plotUpdateTimer->start();
    }
}

void MainWindow::onPlotUpdateTimer()
{
    m_plotUpdateTimer->stop();

    QCustomPlot *plot = qobject_cast<QCustomPlot*>(ui->plotWidget);
    if(!plot || m_dataList.isEmpty()) return;

    // 准备数据
    QVector<double> x(m_dataList.size()), y1(m_dataList.size()), y2(m_dataList.size()), y3(m_dataList.size());

    for(int i = 0; i < m_dataList.size(); ++i)
    {
        x[i] = i;  // 使用索引作为X轴
        y1[i] = m_dataList[i].distance;
        y2[i] = m_dataList[i].height;
        y3[i] = m_dataList[i].horizontal;
    }

    // 设置数据
    plot->graph(0)->setData(x, y1);
    plot->graph(1)->setData(x, y2);
    plot->graph(2)->setData(x, y3);
    plot->replot();
}

// 连接检测
void MainWindow::on_pushButton_connect_clicked()
{
    sendCommand("AT+Connect");
}

void MainWindow::onConnectTimer()
{
    sendCommand("OK+Distance");
}

// 模式设置
void MainWindow::on_pushButton_3_clicked()
{
    int mode = ui->comboBox_mode->currentIndex();
    m_lastModeIdx = mode;
    m_lastMode = ui->comboBox_mode->currentText();
    sendModeCommand(mode);
}

void MainWindow::sendModeCommand(int mode)
{
    if(mode < 0 || mode > 3) return;

    m_currentMode = mode;
    QString cmd = QString("AT+Mode=%1").arg(mode);
    sendCommand(cmd);

    updateModeIcon(mode);
    updateDisplayGroup(-999999, -999999, -999999);
}

// 测量控制
void MainWindow::on_pushButton_4_clicked()  // 单次测量
{
    sendMeasureCommand(0);
}

void MainWindow::on_pushButton_5_clicked()  // 连续测量
{
    sendMeasureCommand(1);
    setControlsEnabled(false);
}

void MainWindow::on_pushButton_stop_clicked()  // 停止测量
{
    sendStopCommand();
    setControlsEnabled(true);
}

void MainWindow::sendMeasureCommand(int type)
{
    QString cmd = QString("AT+Measure=%1").arg(type);
    sendCommand(cmd);
    m_isMeasuring = (type == 1);

    if(type == 1) {
        ui->statusbar->showMessage(QStringLiteral("连续测量中..."));
    }
}

void MainWindow::sendStopCommand()
{
    sendCommand("AT+Stop");
    m_isMeasuring = false;
    ui->statusbar->showMessage(QStringLiteral("测量停止"));
}

// 亮度设置
void MainWindow::on_pushButton_brightness_clicked()
{
    int level = ui->comboBox_brightness->currentIndex() + 1;  // 1-4
    sendBrightnessCommand(level);
    updateBrightnessIcon(level);
}

void MainWindow::sendBrightnessCommand(int level)
{
    if(level < 1 || level > 4) return;

    QString cmd = QString("AT+Brightness=%1").arg(level);
    sendCommand(cmd);
}

// 单位设置
void MainWindow::on_pushButton_unit_clicked()
{
    int unit = ui->comboBox_unit->currentIndex();
    m_lastUnitIdx = unit;
    m_lastUnit = ui->comboBox_unit->currentIndex() == 0 ? "m" : "y";
    sendUnitCommand(unit);
    updateUnitIcons(unit);
}

void MainWindow::sendUnitCommand(int unit)
{
    if(unit < 0 || unit > 1) return;

    QString cmd = QString("AT+Unit=%1\r\n").arg(unit);
    sendCommand(cmd);

    ui->DIS0->clear();    // 清空显示
    ui->DIS1->clear();
    ui->DIS2->clear();

    ui->DIS0->setText("---.-");
    ui->DIS1->setText("---.-");
    ui->DIS2->setText("---.-");

}

// 马达设置
void MainWindow::on_pushButton_motor_clicked()
{
    int on = ui->comboBox_motor->currentIndex();
    sendMotorCommand(on);
    updateMotorIcon(on == 1);

}

void MainWindow::sendMotorCommand(int on)
{
    if(on < 0 || on > 1) return;

    QString cmd = QString("AT+Motor=%1").arg(on);
    sendCommand(cmd);
}

// 更新电池显示
void MainWindow::updateBatteryDisplay()
{
    QString text = "电池电量: ";
    switch(m_batteryLevel) {
        case 0: text += "▌ 0% (空)"; break;
        case 1: text += "▌▌ 25%"; break;
        case 2: text += "▌▌▌ 50%"; break;
        case 3: text += "▌▌▌▌ 75%"; break;
        case 4: text += "▌▌▌▌▌ 100%"; break;
        default: text += "--";
    }
    ui->label_battery->setText(text);
}

// 日志管理
void MainWindow::on_pushButton_save_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        "保存日志",
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt",
        "文本文件 (*.txt);;CSV文件 (*.csv)");

    if(!filename.isEmpty()) {
        saveToFile(filename);
    }
}

void MainWindow::on_pushButton_clear_clicked()
{
    //ui->plainTextEdit_log->clear();
    ui->tableWidget->setRowCount(0);
    m_dataList.clear();

    QCustomPlot *plot = qobject_cast<QCustomPlot*>(ui->plotWidget);
    if(plot) {
        plot->graph(0)->data()->clear();
        plot->graph(1)->data()->clear();
        plot->graph(2)->data()->clear();
        plot->replot();
    }

    ui->DIS0->setText("---.-");
    ui->DIS1->setText("---.-");
    ui->DIS2->setText("---.-");

    ui->action_save->setEnabled(false);

    addToLog("显示已清除", false);
}

void MainWindow::saveToFile(const QString &filename)
{
    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法创建文件"));
        return;
    }

    QTextStream out(&file);

    if(filename.endsWith(".csv", Qt::CaseInsensitive)) {

        // CSV格式

        out << QStringLiteral("时间,主显示参数,辅显参数I,辅显参数II,模式\n");
        for(const MeasureData &md : m_dataList) {
            QString unitStr = (md.unit == 0) ? "m" : "y";

            QString d1, d2;
            if (md.mode == 0 || md.mode == 1) {
                d1 = "/";   // 强制覆盖
                d2 = "/";
            } else {
                d1 = QString::number(md.height, 'f', 1)+unitStr;
                d2 = QString::number(md.horizontal, 'f', 1)+unitStr;
            }

            out << md.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz") << ","
                << md.distance << unitStr << ","
                << d1 << ","
                << d2 << ","
                << modeMap.value(md.mode)<< "\n";

        }
    }
    else
    {
        // TXT格式
        out << "SNDWAY SW-3000A&B - " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
        out << "========================================\n\n";
        for(const MeasureData &md : m_dataList) {
            QString unitStr = (md.unit == 0) ? "m" : "y";


            QString d1, d2;
            if (md.mode == 0 || md.mode == 1) {
                d1 = "/";
                d2 = "/";
            } else {
                d1 = QString::number(md.height, 'f', 1)+unitStr;
                d2 = QString::number(md.horizontal, 'f', 1)+unitStr;
            }

            out << QStringLiteral("时间: ") << md.timestamp.toString("hh:mm:ss.zzz") << "\n";
            out << QStringLiteral("主显示参数: ") << md.distance << " " << unitStr << "\n";
            out << QStringLiteral("辅显参数I: ") << d1 << "\n";
            out << QStringLiteral("辅显参数II: ") << d2 << "\n";
            out << QStringLiteral("模式: ") << modeMap.value(md.mode) << "\n";
            out << "----------------------------------------\n";
        }
    }

    file.close();
    QMessageBox::information(this, QStringLiteral("完成"), QStringLiteral("数据已保存到：%1").arg(filename));
}

// 曲线控制
void MainWindow::on_checkBox_autoScroll_toggled(bool checked)
{
    Q_UNUSED(checked);
    // 已在updatePlot中处理
}

void MainWindow::on_pushButton_clearPlot_clicked()
{
    QCustomPlot *plot = qobject_cast<QCustomPlot*>(ui->plotWidget);
    if(plot) {
        plot->graph(0)->data()->clear();
        plot->graph(1)->data()->clear();
        plot->graph(2)->data()->clear();
        plot->replot();
    }
}

// 菜单动作
void MainWindow::on_action_open_triggered()
{
    QString filename = QFileDialog::getOpenFileName(this, QStringLiteral("打开配置文件"), "", QStringLiteral("配置文件 (*.ini *.conf)"));
    if(!filename.isEmpty()) {
        // 加载配置
        QSettings settings(filename, QSettings::IniFormat);
        // 读取配置...
    }
}

void MainWindow::on_action_save_triggered()
{
    saveConfig();
    QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("配置已保存"));
}

void MainWindow::on_action_exit_triggered()
{
    close();
}

void MainWindow::on_action_portSettings_triggered()
{
    updateSerialPortList();
    QMessageBox::information(this, QStringLiteral("串口设置"), QStringLiteral("请在上方串口选择区域设置参数"));
}

void MainWindow::on_action_about_triggered()
{
    QMessageBox::about(this, "关于",
        "SNDWAY SW-3000A&B电脑软件 v1.0\n\n"
        "支持功能：\n"
        "- 串口通信 (115200, 8N1)\n"
        "- 模式设置 (扫描/锁定/测高/方位角)\n"
        "- 单次/连续测量\n"
        "- 亮度/单位/马达控制\n"
        "- 数据表格与曲线显示\n"
        "- 记录保存\n\n"
        "- 基于 Qt 5.15.2 和 QCustomPlot 开发\n"
        "- 本程序使用Qt5开源库（LGPLv3协议，动态链接）、QCustomPlot绘图库（GPLv3）\n"
        "- 本软件完整开源代码仓库:https://github.com/uuuuiz/qaq\n"
        "- Qt官方源码可前往 https://download.qt.io/official_releases/qt/ 获取\n"
        "- 软件目录附带 GPLv3.txt、LGPLv3.txt 协议文件");
}

// 配置保存/加载
void MainWindow::saveConfig()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.setValue("Serial/Port", ui->comboBox_SerialPortInfo->currentText());
    settings.setValue("Serial/BaudRate", ui->comboBox_baud->currentText());
    settings.setValue("Settings/Mode", ui->comboBox_mode->currentIndex());
    settings.setValue("Settings/Brightness", ui->comboBox_brightness->currentIndex());
    settings.setValue("Settings/Unit", ui->comboBox_unit->currentIndex());
    settings.setValue("Settings/Motor", ui->comboBox_motor->currentIndex());
}

void MainWindow::loadConfig()
{
    QSettings settings("config.ini", QSettings::IniFormat);

    QString port = settings.value("Serial/Port").toString();
    int baudIndex = settings.value("Serial/BaudRate", "115200").toString().toInt();

    // 设置保存的值
    int mode = settings.value("Settings/Mode", 0).toInt();
    int brightness = settings.value("Settings/Brightness", 0).toInt();
    int unit = settings.value("Settings/Unit", 0).toInt();
    int motor = settings.value("Settings/Motor", 0).toInt();

    ui->comboBox_mode->setCurrentIndex(mode);
    ui->comboBox_brightness->setCurrentIndex(brightness);
    ui->comboBox_unit->setCurrentIndex(unit);
    ui->comboBox_motor->setCurrentIndex(motor);

    m_lastModeIdx = mode;
    m_lastMode = ui->comboBox_mode->currentText();
    m_lastUnitIdx = ui->comboBox_unit->currentIndex();
    m_lastUnit = m_lastUnitIdx == 0 ? "m" : "y";
}

void MainWindow::closeEvent(QCloseEvent *event)
{

    if (m_dataList.isEmpty())
    {
        event->accept();
        return;
    }


    QMessageBox::StandardButton reply = QMessageBox::question(this, QStringLiteral("退出"),
        QStringLiteral("是否保存当前数据？"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if(reply == QMessageBox::Yes) {
        QString filename = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".csv";
        saveToFile(filename);
        event->accept();
    } else if(reply == QMessageBox::No) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::on_pushButton_clicked()
{
    // 弹出文件选择对话框
     QString fileName = QFileDialog::getOpenFileName(nullptr,
         "选择一个文件",                    // 对话框标题
         "/home",                           // 初始目录
         "所有文件 (*.*)" // 文件过滤器，多个类型用 ;; 分隔
     );


     // 判断用户是否选择了文件（未选择则返回空字符串）
     if (!fileName.isEmpty())
     {
         qDebug() << "你选择的文件是：" << fileName;
         qDebug() << extractAttachmentPathsAfterBuiltinLibrary(fileName);
     } else
     {
         qDebug() << "用户取消了选择";
     }
}

QStringList MainWindow::getAttachmentPaths(const QString& xmlFilePath)
{
    QStringList paths;

    QFile file(xmlFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "无法打开文件:" << xmlFilePath;
        return paths;
    }

    QByteArray data = file.readAll();
    file.close();

    // 尝试多种编码
    QList<QByteArray> codecs = {"GBK", "GB2312", "GB18030", "UTF-8", "System"};
    QString content;
    QString usedCodec;

    for (const QByteArray& codecName : codecs) {
        QTextCodec* codec = QTextCodec::codecForName(codecName);
        if (codec) {
            QString temp = codec->toUnicode(data);
            // 检查是否包含预期的中文字符
            if (temp.contains("Attachment") &&
                (temp.contains("内置库") || temp.contains("01NVH库"))) {
                content = temp;
                usedCodec = QString::fromUtf8(codecName);
                break;
            }
        }
    }

    if (content.isEmpty()) {
        // 如果都没有找到，使用默认的UTF-8
        content = QString::fromUtf8(data);
        usedCodec = "UTF-8 (default)";
    }

    qDebug() << "成功使用编码:" << usedCodec << "解码";

    // 使用正则表达式提取所有Attachment标签的内容
    QRegularExpression re("<Attachment\\d+>([^<]+)</Attachment\\d+>");
    QRegularExpressionMatchIterator i = re.globalMatch(content);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString fullPath = match.captured(1).trimmed();

        // 清理路径中的特殊字符
        fullPath = fullPath.replace(QRegularExpression("[\\x00-\\x08\\x0B-\\x0C\\x0E-\\x1F]"), "");

        // 查找"内置库"的位置
        int index = fullPath.indexOf("内置库");
        if (index != -1) {
            QString newPath = fullPath.mid(index);
            paths.append(newPath);
            qDebug() << "提取的路径:" << newPath;
        } else {
            // 如果没找到，尝试查找"01NVH库"
            int nvhIndex = fullPath.indexOf("01NVH库");
            if (nvhIndex != -1) {
                QString newPath = "内置库/" + fullPath.mid(nvhIndex);
                paths.append(newPath);
                qDebug() << "使用01NVH库构建:" << newPath;
            } else {
                // 最后的备选：只取文件名
                QFileInfo fileInfo(fullPath);
                paths.append("内置库/" + fileInfo.fileName());
                qDebug() << "使用文件名:" << "内置库/" + fileInfo.fileName();
            }
        }
    }

    // 去重
    paths.removeDuplicates();

    qDebug() << "总共找到路径数量:" << paths.size();
    return paths;
}


#include <QDomDocument>
#include <QFile>
#include <QStringList>
#include <QDebug>

QStringList MainWindow::extractLibraryPaths(const QString& xmlFilePath)
{
    QStringList result;

    QFile file(xmlFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file";
        return result;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        qDebug() << "Failed to parse XML";
        file.close();
        return result;
    }
    file.close();

    QDomNodeList allNodes = doc.elementsByTagName("*");

    for (int i = 0; i < allNodes.count(); ++i) {
        QDomNode node = allNodes.at(i);

        // 只处理 Attachment 开头的标签
        if (node.nodeName().startsWith("Attachment")) {
            QString fullPath = node.toElement().text();

            int index = fullPath.indexOf("内置库/");
            if (index != -1) {
                QString subPath = fullPath.mid(index);
                result.append(subPath);
            }
        }
    }

    return result;
}

#include <QString>
#include <QFile>
#include <QDomDocument>
#include <QDebug>
#include <QRegularExpression>

QStringList MainWindow::extractAttachmentPathsAfterBuiltinLibrary(const QString& xmlFilePath)
{
    QStringList resultPaths;

    // 打开XML文件
    QFile file(xmlFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件:" << file.errorString();
        return resultPaths;
    }

    // 解析XML
    QDomDocument doc;
    QString errorMsg;
    int errorLine, errorColumn;
    if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn)) {
        qWarning() << "XML解析失败:" << errorMsg << "行:" << errorLine << "列:" << errorColumn;
        file.close();
        return resultPaths;
    }
    file.close();

    // 获取根元素
    QDomElement root = doc.documentElement();
    if (root.tagName() != "FailureSceneData") {
        qWarning() << "根元素不是 FailureSceneData";
        return resultPaths;
    }

    // 递归遍历所有节点，查找以"Attachment"开头的标签
    std::function<void(const QDomElement&)> traverseNodes = [&](const QDomElement& elem) {
        QDomElement child = elem.firstChildElement();
        while (!child.isNull()) {
            QString tagName = child.tagName();
            // 检查是否以"Attachment"开头（如Attachment1, Attachment2, Attachment3...）
            if (tagName.startsWith("Attachment")) {
                QString path = child.text().trimmed();
                if (!path.isEmpty()) {
                    // 查找"内置库"之后的部分
                    int index = path.indexOf(QStringLiteral("内置库")) + 4;
                    if (index != -1)
                    {
                        // 提取从"内置库"开始到末尾的部分
                        QString extracted = path.mid(index);
                        resultPaths.append(extracted);
                    }
                    else
                    {
                        // 如果没有"内置库"，则保留原始路径
                        resultPaths.append(path);
                    }
                }
            }
            // 递归处理子元素
            traverseNodes(child);
            child = child.nextSiblingElement();
        }
    };

    // 开始遍历根元素下的所有子节点
    traverseNodes(root);

    return resultPaths;
}

void MainWindow::updateModeIcon(int mode)
{

    ui->MODE->setStyleSheet(R"(
        background: #4A5568;
        border-radius: 6px;
        border: 1px solid #718096;
        padding: 3px;
        color: white;
    )");

    // 居中显示
    ui->MODE->setAlignment(Qt::AlignCenter);


    QString path;
    switch(mode) {
        case 0: path = ":/new/prefix1/image/mode1.png"; break;
        case 1: path = ":/new/prefix1/image/mode2.png"; break;
        case 3: path = ":/new/prefix1/image/mode3.png"; break;
        case 2: path = ":/new/prefix1/image/mode 4.png"; break;
        default: path = ":/new/prefix1/image/mode1.png"; break;
    }

    // 加载并显示图片
    QPixmap pix(path);
    if (!pix.isNull()) {
        ui->MODE->setPixmap(pix.scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        ui->MODE->setScaledContents(true);
    }
}

void MainWindow::updateBrightnessIcon(int level)
{

    ui->Brightness->setStyleSheet(R"(
        background: #4A5568;
        border-radius: 6px;
        border: 1px solid #718096;
        padding: 3px;
        color: white;
    )");

    // 居中显示
    ui->Brightness->setAlignment(Qt::AlignCenter);

    // 根据亮度等级选择图片
    QString path;
    switch(level) {
        case 1:  path = ":/new/prefix1/image/luminance1.png"; break;
        case 2:  path = ":/new/prefix1/image/luminance2.png"; break;
        case 3:  path = ":/new/prefix1/image/luminance3.png"; break;
        case 4:  path = ":/new/prefix1/image/luminance4.png"; break;
        default: path = ":/new/prefix1/image/luminance1.png"; break;
    }


    QPixmap pix(path);
    if (!pix.isNull()) {
        ui->Brightness->setPixmap(pix.scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        ui->Brightness->setScaledContents(true);
    }
}

// ================== 马达状态图标更新 ==================
void MainWindow::updateMotorIcon(bool isOn)
{

    ui->Motor->setStyleSheet(R"(
        background: #4A5568;
        border-radius: 6px;
        border: 1px solid #718096;
        padding: 3px;
        color: white;
    )");

    ui->Motor->setAlignment(Qt::AlignCenter);

    QString path;
    if (isOn) {
        path = ":/new/prefix1/image/shakeon.png";   // 开
    } else {
        path = ":/new/prefix1/image/shakeoff.png";  // 关
    }

    QPixmap pix(path);
    if (!pix.isNull()) {
        ui->Motor->setPixmap(pix.scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        ui->Motor->setScaledContents(true);
    }
}

// ================== 实时显示测量值到 GroupBox ==================
void MainWindow::updateDisplayGroup(double dis0, double dis1, double dis2)
{
    // === DIS0 ===
    if (dis0 == -999999)
        ui->DIS0->setText("---.-");
    else
        ui->DIS0->setText(QString::number(dis0, 'f', 1));

    // === DIS1 ===
    if (dis1 == -999999)
        ui->DIS1->setText("---.-");
    else
        ui->DIS1->setText(QString::number(dis1, 'f', 1));

    // === DIS2 ===
    if (dis2 == -999999)
        ui->DIS2->setText("---.-");
    else
        ui->DIS2->setText(QString::number(dis2, 'f', 1));
}

// ================== 电池图标显示 ==================
void MainWindow::updateBatteryIcon(int batteryLevel)
{

    ui->Batt->setStyleSheet(R"(
        background: #4A5568;
        border-radius: 6px;
        border: 1px solid #718096;
        padding: 3px;
        color: white;
    )");

    ui->Batt->setAlignment(Qt::AlignCenter);

    QString path;
    switch (batteryLevel) {
        case 0:   path = ":/new/prefix1/image/Batt0.png"; break;   // 0%
        case 1:   path = ":/new/prefix1/image/Batt1.png"; break;   // 25%
        case 2:   path = ":/new/prefix1/image/Batt2.png"; break;   // 50%
        case 3:   path = ":/new/prefix1/image/Batt3.png"; break;   // 75%
        case 4:   path = ":/new/prefix1/image/Batt4.png"; break;   // 100%
        default:  path = ":/new/prefix1/image/Batt4.png"; break;
    }

    QPixmap pix(path);
    if (!pix.isNull()) {
        ui->Batt->setPixmap(pix.scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        ui->Batt->setScaledContents(true);
    }
}

void MainWindow::updateUnitIcons(int unit)
{

    QString style = R"(
        background: #4A5568;
        border-radius: 6px;
        border: 1px solid #718096;
        padding: 3px;
        color: white;
    )";

    ui->Unit1->setStyleSheet(style);
    ui->Unit2->setStyleSheet(style);
    ui->Unit3->setStyleSheet(style);

    ui->Unit1->setAlignment(Qt::AlignCenter);
    ui->Unit2->setAlignment(Qt::AlignCenter);
    ui->Unit3->setAlignment(Qt::AlignCenter);

    // 选择图片
    QString path;
    if (unit == 0) {
        path = ":/new/prefix1/image/m.png";    // 米
    } else {
        path = ":/new/prefix1/image/y.png";    // 码
    }

    // 加载并设置到 3 个标签
    QPixmap pix(path);
    if (!pix.isNull()) {
        ui->Unit1->setPixmap(pix.scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        ui->Unit2->setPixmap(pix.scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        ui->Unit3->setPixmap(pix.scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        ui->Unit1->setScaledContents(true);
        ui->Unit2->setScaledContents(true);
        ui->Unit3->setScaledContents(true);
    }
}

// ================== 控制测量相关控件的启用/禁用状态 ==================
void MainWindow::setControlsEnabled(bool enabled)
{
    // 所有需要在连续测量时禁用的下拉框
    ui->comboBox_SerialPortInfo->setEnabled(enabled);
    ui->comboBox_baud->setEnabled(enabled);
    ui->comboBox_brightness->setEnabled(enabled);
    ui->comboBox_mode->setEnabled(enabled);
    ui->comboBox_motor->setEnabled(enabled);
    ui->comboBox_unit->setEnabled(enabled);


}

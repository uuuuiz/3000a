#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QCloseEvent>

// QCustomPlot 头文件
#include "qcustomplot.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 数据点结构体
struct MeasureData {
    QDateTime timestamp;
    double distance = 0.0;    // 直线距离
    double height = 0.0;      // 高度
    double horizontal = 0.0;  // 水平距离
    double angle = 0.0;       // 角度
    int mode;           // 当前模式
    int unit;          // 当前单位
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

    QStringList getAttachmentPaths(const QString& xmlFilePath);

private slots:
    // 串口相关
    void on_pushButton_open_clicked();
    void on_pushButton_2_clicked();  // 关闭串口
    void slot_onSerialDataReceived();
    void on_pushButton_connect_clicked();  // 检测连接

    // 模式设置
    void on_pushButton_3_clicked();  // 设定模式

    // 测量控制
    void on_pushButton_4_clicked();  // 单次测量
    void on_pushButton_5_clicked();  // 连续测量
    void on_pushButton_stop_clicked();  // 停止测量

    // 参数设置
    void on_pushButton_brightness_clicked();
    void on_pushButton_unit_clicked();
    void on_pushButton_motor_clicked();

    // 日志管理
    void on_pushButton_save_clicked();  // 保存日志
    void on_pushButton_clear_clicked(); // 清除接收

    // 曲线控制
    void on_checkBox_autoScroll_toggled(bool checked);
    void on_pushButton_clearPlot_clicked();

    // 菜单动作
    void on_action_open_triggered();
    void on_action_save_triggered();
    void on_action_exit_triggered();
    void on_action_portSettings_triggered();
    void on_action_about_triggered();

    // 定时器
    void onConnectTimer();
    void onPlotUpdateTimer();

    void on_pushButton_clicked();
    QStringList extractLibraryPaths(const QString& xmlFilePath);
    QStringList extractAttachmentPathsAfterBuiltinLibrary(const QString& xmlFilePath);

    void updateModeIcon(int mode);
    void updateBrightnessIcon(int level);
    void updateMotorIcon(bool isOn);
    void updateBatteryIcon(int batteryLevel);
    void updateUnitIcons(int unit);
    void updateDisplayGroup(double dis0, double dis1, double dis2);

private:
    void setControlsEnabled(bool enabled); // 控制下拉框等控件的启用状态

private:
    // 私有函数
    void initUI();
    void initSerialPort();
    void initPlot();
    void initConnections();
    void updateSerialPortList();
    void sendCommand(const QString &cmd);
    void parseData(const QString &data);
    void addToLog(const QString &text, bool isReceived = true);
    void updateTable();
    void updatePlot();
    void updateBatteryDisplay();
    void saveToFile(const QString &filename);
    void loadConfig();
    void saveConfig();

    // 命令发送函数
    void sendModeCommand(int mode);
    void sendMeasureCommand(int type);  // 0:单次, 1:连续
    void sendStopCommand();
    void sendConnectCommand();
    void sendBrightnessCommand(int level);
    void sendUnitCommand(int unit);
    void sendMotorCommand(int on);

private:
    Ui::MainWindow *ui;

    // 串口对象
    QSerialPort m_serial;

    // 定时器
    QTimer *m_connectTimer;      // 连接检测定时器
    QTimer *m_plotUpdateTimer;    // 曲线更新定时器
    QTimer *m_serialRefreshTimer;

    // 数据缓冲区
    QByteArray m_receiveBuffer;

    // 当前状态
    bool m_isConnected;
    bool m_isMeasuring;
    int m_currentMode;
    int m_currentUnit;
    int m_batteryLevel;

    bool m_istrue = false;

    // 数据列表
    QList<MeasureData> m_dataList;

    // 日志文件
    QFile *m_logFile;
    QTextStream *m_logStream;
    // 定义映射表
    QMap<int, QString> modeMap = {
        {0, "扫描测距模式"},
        {1, "目标锁定模式"},
        {2, "测高模式"},
        {3, "方位角模式"},
    };

    //最新一次的单位
    QString m_lastUnit;
    int m_lastUnitIdx;

    //最新一次的模式
    QString m_lastMode;
    int m_lastModeIdx;
};



#endif // MAINWINDOW_H

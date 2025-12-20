#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QTimer>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 串口相关
    void refreshSerialPorts();
    void toggleSerialPort();
    void readSerialData();
    void handleError(QSerialPort::SerialPortError error);

    void sendCommand(QString cmd);

    // 核心功能
    void onBtnGetAllParams();   // 新增：一键获取所有参数

    // 设置指令槽函数
    void onBtnSetCfg();         // AT+SETCFG
    void onBtnSetPan();         // AT+SETPAN
    void onBtnSetAnt();         // AT+SETANT
    void onBtnSetPow();         // AT+SETPOW
    void onBtnSetCap();         // AT+SETCAP
    void onBtnSetRpt();         // AT+SETRPT
    void onBtnSleep();          // AT+SLEEP
    void onBtnSendData();       // AT+DATA

    // 系统指令
    void onBtnCheckConn();
    void onBtnRestart();
    void onBtnRestore();
    void onBtnSave();

    void onBtnClearLog();

private:
    void setupUi();
    void setupConnections();
    void parseLine(const QString &line); // 解析器

    // UI 组件指针
    QWidget *centralWidget;
    QVBoxLayout *mainLayout; // 改为垂直布局作为主框架

    // --- 1. 串口控制区 ---
    QGroupBox *groupSerial;
    QComboBox *comboPort;
    QComboBox *comboBaud;
    QPushButton *btnOpenClose;
    QPushButton *btnRefresh;

    // --- 2. 左侧：状态看板 (显示专用) ---
    QLineEdit *dispVersion;
    QLineEdit *dispDevId;
    QLineEdit *dispRole;
    QLineEdit *dispRate;
    QLineEdit *dispFilter;
    QLineEdit *dispPanId;
    QLineEdit *dispAntDelay;
    QLineEdit *dispPower;
    QLineEdit *dispCapacity; // 显示标签数/槽时间/模式
    QLineEdit *dispRptStatus;
    QPushButton *btnGetAll; // 一键获取按钮

    // --- 3. 右侧：配置面板 (输入专用) ---
    // 基础配置
    QLineEdit *inputDevId;
    QComboBox *inputRole;
    QComboBox *inputRate;
    QCheckBox *inputFilter;
    QPushButton *btnSetCfg;

    QLineEdit *inputPanId;
    QPushButton *btnSetPan;

    // 射频配置
    QLineEdit *inputAntDelay;
    QPushButton *btnSetAnt;

    QLineEdit *inputPower;
    QPushButton *btnSetPow;

    // 容量配置
    QLineEdit *inputTagCount;
    QLineEdit *inputSlotTime;
    QComboBox *inputExtMode;
    QPushButton *btnSetCap;

    // 运行控制
    QCheckBox *inputAutoRpt;
    QPushButton *btnSetRpt;

    QLineEdit *inputSleepTime;
    QPushButton *btnSleep;

    QLineEdit *inputDataLen;
    QLineEdit *inputDataContent;
    QPushButton *btnSendData;

    // --- 4. 系统指令区 ---
    QPushButton *btnCheck;
    QPushButton *btnRestart;
    QPushButton *btnRestore;
    QPushButton *btnSave;

    // --- 5. 日志区 ---
    QTextEdit *textLog;
    QCheckBox *checkHexDisplay;
    QPushButton *btnClearLog;

    // 逻辑变量
    QSerialPort *serial;
    QByteArray m_buffer;
};

#endif // MAINWINDOW_H

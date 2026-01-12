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
    void onBtnGetAllParams();   // 一键获取所有参数
    void executeNextCommand();  // 执行队列中的下一条指令
    void onResponseTimeout();   // 响应超时处理

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
    bool isResponseForCmd(const QString &line, const QString &cmd); // 检查回复是否匹配当前指令

    // UI 组件指针
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;

    // --- 1. 串口控制区 ---
    QGroupBox *groupSerial;
    QComboBox *comboPort;
    QComboBox *comboBaud;
    QPushButton *btnOpenClose;
    QPushButton *btnRefresh;

    // --- 2. 左侧：状态看板 ---
    QLineEdit *dispVersion;
    QLineEdit *dispDevId;
    QLineEdit *dispRole;
    QLineEdit *dispRate;
    QLineEdit *dispFilter;
    QLineEdit *dispPanId;
    QLineEdit *dispAntDelay;
    QLineEdit *dispPower;
    QLineEdit *dispCapacity;
    QLineEdit *dispRptStatus;
    QPushButton *btnGetAll;

    // --- 3. 右侧：配置面板 ---
    QLineEdit *inputDevId;
    QComboBox *inputRole;
    QComboBox *inputRate;
    QCheckBox *inputFilter;
    QPushButton *btnSetCfg;

    QLineEdit *inputPanId;
    QPushButton *btnSetPan;

    QLineEdit *inputAntDelay;
    QPushButton *btnSetAnt;

    QLineEdit *inputPower;
    QPushButton *btnSetPow;

    QLineEdit *inputTagCount;
    QLineEdit *inputSlotTime;
    QComboBox *inputExtMode;
    QPushButton *btnSetCap;

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

    // --- 健壮的指令队列管理 ---
    QTimer *m_responseTimer;     // 超时定时器
    QStringList m_cmdQueue;      // 待发送指令队列
    QString m_currentCmd;        // 当前正在等待回复的指令
    int m_retryCount;            // 当前指令已重试次数
    const int MAX_RETRIES = 3;   // 最大重试次数
};

#endif // MAINWINDOW_H

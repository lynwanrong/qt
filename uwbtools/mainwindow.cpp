#include "mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , serial(new QSerialPort(this))
{
    setupUi();
    setupConnections();
    refreshSerialPorts();

    setWindowTitle(tr("Makerfabs UWB Configuration Tool"));
    resize(1100, 800);
}

MainWindow::~MainWindow()
{
    if (serial->isOpen()) {
        serial->close();
    }
}

void MainWindow::setupUi()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    mainLayout = new QVBoxLayout(centralWidget);

    // ============================================================
    // 1. 顶部：串口设置 & 系统指令
    // ============================================================
    QHBoxLayout *topLayout = new QHBoxLayout();

    // 串口设置组
    groupSerial = new QGroupBox(tr("Connection Settings"), this);
    QHBoxLayout *serialLayout = new QHBoxLayout(groupSerial);
    comboPort = new QComboBox(this);
    comboBaud = new QComboBox(this);
    comboBaud->addItems({"115200", "9600", "38400", "57600", "921600"});
    comboBaud->setCurrentText("115200");
    btnRefresh = new QPushButton(tr("Refresh"), this);
    btnOpenClose = new QPushButton(tr("Open Port"), this);
    serialLayout->addWidget(new QLabel(tr("Port:")));
    serialLayout->addWidget(comboPort);
    serialLayout->addWidget(new QLabel(tr("Baud:")));
    serialLayout->addWidget(comboBaud);
    serialLayout->addWidget(btnRefresh);
    serialLayout->addWidget(btnOpenClose);

    // 系统指令组
    QGroupBox *groupSys = new QGroupBox(tr("System Operations"), this);
    QHBoxLayout *sysLayout = new QHBoxLayout(groupSys);
    btnCheck = new QPushButton(tr("Check Conn(AT?)"), this);
    btnRestart = new QPushButton(tr("Restart Module"), this);
    btnRestore = new QPushButton(tr("Factory Reset"), this);
    btnSave = new QPushButton(tr("Save Config(Flash)"), this);

    btnRestore->setStyleSheet("color: red;");
    btnSave->setStyleSheet("font-weight: bold; color: blue;");

    sysLayout->addWidget(btnCheck);
    sysLayout->addWidget(btnRestart);
    sysLayout->addWidget(btnRestore);
    sysLayout->addWidget(btnSave);

    topLayout->addWidget(groupSerial, 1); // 比例 1
    topLayout->addWidget(groupSys, 1);    // 比例 1
    mainLayout->addLayout(topLayout);

    // ============================================================
    // 2. 中部：左右分栏 (左：状态看板，右：配置面板)
    // ============================================================
    QHBoxLayout *centerLayout = new QHBoxLayout();

    // ---------------- 左侧：设备状态看板 (Display) ----------------
    QGroupBox *groupStatus = new QGroupBox(tr("Current Device Status"), this);
    groupStatus->setStyleSheet("QGroupBox { font-weight: bold; } QLineEdit { background-color: #f0f0f0; color: #333; }");
    QFormLayout *statusLayout = new QFormLayout(groupStatus);

    btnGetAll = new QPushButton(tr("🔄 Fetch All Parameters"), this);
    btnGetAll->setStyleSheet("background-color: #e1f5fe; font-weight: bold; padding: 5px;");
    btnGetAll->setMinimumHeight(35);

    dispVersion = new QLineEdit(this); dispVersion->setReadOnly(true);
    dispDevId = new QLineEdit(this); dispDevId->setReadOnly(true);
    dispRole = new QLineEdit(this); dispRole->setReadOnly(true);
    dispRate = new QLineEdit(this); dispRate->setReadOnly(true);
    dispFilter = new QLineEdit(this); dispFilter->setReadOnly(true);
    dispPanId = new QLineEdit(this); dispPanId->setReadOnly(true);
    dispAntDelay = new QLineEdit(this); dispAntDelay->setReadOnly(true);
    dispPower = new QLineEdit(this); dispPower->setReadOnly(true);
    dispCapacity = new QLineEdit(this); dispCapacity->setReadOnly(true);
    dispRptStatus = new QLineEdit(this); dispRptStatus->setReadOnly(true);

    statusLayout->addRow(btnGetAll);
    statusLayout->addRow(new QLabel(tr("Firmware Ver:")), dispVersion);
    statusLayout->addRow(new QLabel(tr("Device ID:")), dispDevId);
    statusLayout->addRow(new QLabel(tr("Device Role:")), dispRole);
    statusLayout->addRow(new QLabel(tr("Air Rate:")), dispRate);
    statusLayout->addRow(new QLabel(tr("Dist Filter:")), dispFilter);
    statusLayout->addRow(new QLabel(tr("Network ID (PAN):")), dispPanId);
    statusLayout->addRow(new QLabel(tr("Antenna Delay:")), dispAntDelay);
    statusLayout->addRow(new QLabel(tr("Tx Power:")), dispPower);
    statusLayout->addRow(new QLabel(tr("System Cap:")), dispCapacity);
    statusLayout->addRow(new QLabel(tr("Auto-Report:")), dispRptStatus);

    // ---------------- 右侧：参数配置面板 (Settings) ----------------
    QGroupBox *groupConfig = new QGroupBox(tr("Configuration"), this);
    QVBoxLayout *configVLayout = new QVBoxLayout(groupConfig);

    // A. 基础参数
    QGroupBox *subBasic = new QGroupBox(tr("Basic Parameters"), this);
    QGridLayout *gridBasic = new QGridLayout(subBasic);

    inputDevId = new QLineEdit("0", this); inputDevId->setPlaceholderText("ID");
    inputRole = new QComboBox(this);
    inputRole->addItem("Tag (0)", 0); inputRole->addItem("Anchor (1)", 1);
    inputRate = new QComboBox(this);
    inputRate->addItem("6.8Mbps (1)", 1); inputRate->addItem("850kbps (0)", 0);
    inputFilter = new QCheckBox(tr("Enable Filter"), this); inputFilter->setChecked(true);
    btnSetCfg = new QPushButton(tr("Set CFG"), this);

    inputPanId = new QLineEdit(this); inputPanId->setPlaceholderText("PAN ID");
    btnSetPan = new QPushButton(tr("Set PAN"), this);

    gridBasic->addWidget(new QLabel("ID:"), 0, 0); gridBasic->addWidget(inputDevId, 0, 1);
    gridBasic->addWidget(new QLabel("Role:"), 0, 2); gridBasic->addWidget(inputRole, 0, 3);
    gridBasic->addWidget(new QLabel("Rate:"), 0, 4); gridBasic->addWidget(inputRate, 0, 5);
    gridBasic->addWidget(inputFilter, 0, 6);
    gridBasic->addWidget(btnSetCfg, 0, 7);

    gridBasic->addWidget(new QLabel("PAN ID:"), 1, 0); gridBasic->addWidget(inputPanId, 1, 1);
    gridBasic->addWidget(btnSetPan, 1, 7);

    // B. 射频参数
    QGroupBox *subRf = new QGroupBox(tr("RF Parameters"), this);
    QHBoxLayout *layRf = new QHBoxLayout(subRf);
    inputAntDelay = new QLineEdit("16384", this); inputAntDelay->setPlaceholderText("Delay");
    btnSetAnt = new QPushButton(tr("Set Delay"), this);
    inputPower = new QLineEdit("FD", this); inputPower->setPlaceholderText("Hex");
    btnSetPow = new QPushButton(tr("Set Power"), this);

    layRf->addWidget(new QLabel("Antenna Delay:")); layRf->addWidget(inputAntDelay); layRf->addWidget(btnSetAnt);
    layRf->addSpacing(10);
    layRf->addWidget(new QLabel("Tx Power:")); layRf->addWidget(inputPower); layRf->addWidget(btnSetPow);

    // C. 容量配置
    QGroupBox *subCap = new QGroupBox(tr("Capacity Config"), this);
    QHBoxLayout *layCap = new QHBoxLayout(subCap);
    inputTagCount = new QLineEdit("10", this); inputTagCount->setPlaceholderText("Count");
    inputSlotTime = new QLineEdit("10", this); inputSlotTime->setPlaceholderText("Slot(ms)");
    inputExtMode = new QComboBox(this);
    inputExtMode->addItem("Standard(0)", 0); inputExtMode->addItem("Extended(1)", 1);
    btnSetCap = new QPushButton(tr("Set Cap"), this);

    layCap->addWidget(new QLabel("Tag Count:")); layCap->addWidget(inputTagCount);
    layCap->addWidget(new QLabel("Slot Time:")); layCap->addWidget(inputSlotTime);
    layCap->addWidget(new QLabel("Mode:")); layCap->addWidget(inputExtMode);
    layCap->addWidget(btnSetCap);

    // D. 运行控制
    QGroupBox *subRun = new QGroupBox(tr("Runtime Tets"), this);
    QGridLayout *gridRun = new QGridLayout(subRun);
    inputAutoRpt = new QCheckBox(tr("Auto Report"), this);
    btnSetRpt = new QPushButton(tr("Set Report"), this);

    inputSleepTime = new QLineEdit("1000", this); inputSleepTime->setPlaceholderText("ms");
    btnSleep = new QPushButton(tr("Set Sleep"), this);

    inputDataLen = new QLineEdit("10", this); inputDataLen->setFixedWidth(50);
    inputDataContent = new QLineEdit("1234567890", this);
    btnSendData = new QPushButton(tr("Send Data"), this);

    gridRun->addWidget(inputAutoRpt, 0, 0); gridRun->addWidget(btnSetRpt, 0, 1);
    gridRun->addWidget(new QLabel("Sleep:"), 0, 2); gridRun->addWidget(inputSleepTime, 0, 3); gridRun->addWidget(btnSleep, 0, 4);

    gridRun->addWidget(new QLabel("Passthrough(Len/Data):"), 1, 0);
    QHBoxLayout *dataLay = new QHBoxLayout();
    dataLay->addWidget(inputDataLen); dataLay->addWidget(inputDataContent);
    gridRun->addLayout(dataLay, 1, 1, 1, 3);
    gridRun->addWidget(btnSendData, 1, 4);


    // 添加子面板到右侧布局
    configVLayout->addWidget(subBasic);
    configVLayout->addWidget(subRf);
    configVLayout->addWidget(subCap);
    configVLayout->addWidget(subRun);
    configVLayout->addStretch(); // 底部填充

    // 将左右两栏加入中间层，调整比例
    centerLayout->addWidget(groupStatus, 1); // 左侧占 1 份
    centerLayout->addWidget(groupConfig, 2); // 右侧占 2 份
    mainLayout->addLayout(centerLayout);

    // ============================================================
    // 3. 底部：通信日志
    // ============================================================
    QGroupBox *groupLog = new QGroupBox(tr("Communication Log"), this);
    QVBoxLayout *logLayout = new QVBoxLayout(groupLog);
    textLog = new QTextEdit(this);
    textLog->setReadOnly(true);

    QHBoxLayout *logBtnLayout = new QHBoxLayout();
    checkHexDisplay = new QCheckBox(tr("Hex View"), this);
    btnClearLog = new QPushButton(tr("Clear  Log"), this);
    logBtnLayout->addWidget(checkHexDisplay);
    logBtnLayout->addStretch();
    logBtnLayout->addWidget(btnClearLog);

    logLayout->addWidget(textLog);
    logLayout->addLayout(logBtnLayout);

    mainLayout->addWidget(groupLog);
}

void MainWindow::setupConnections()
{
    // 串口
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    connect(btnOpenClose, &QPushButton::clicked, this, &MainWindow::toggleSerialPort);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);
    connect(serial, &QSerialPort::errorOccurred, this, &MainWindow::handleError);
    // 日志
    connect(btnClearLog, &QPushButton::clicked, this, &MainWindow::onBtnClearLog);

    // 系统
    connect(btnCheck, &QPushButton::clicked, this, &MainWindow::onBtnCheckConn);
    connect(btnRestart, &QPushButton::clicked, this, &MainWindow::onBtnRestart);
    connect(btnRestore, &QPushButton::clicked, this, &MainWindow::onBtnRestore);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::onBtnSave);

    // 配置设置 (Write)
    connect(btnSetCfg, &QPushButton::clicked, this, &MainWindow::onBtnSetCfg);
    connect(btnSetPan, &QPushButton::clicked, this, &MainWindow::onBtnSetPan);
    connect(btnSetAnt, &QPushButton::clicked, this, &MainWindow::onBtnSetAnt);
    connect(btnSetPow, &QPushButton::clicked, this, &MainWindow::onBtnSetPow);
    connect(btnSetCap, &QPushButton::clicked, this, &MainWindow::onBtnSetCap);
    connect(btnSetRpt, &QPushButton::clicked, this, &MainWindow::onBtnSetRpt);
    connect(btnSleep, &QPushButton::clicked, this, &MainWindow::onBtnSleep);
    connect(btnSendData, &QPushButton::clicked, this, &MainWindow::onBtnSendData);

    // 一键获取 (Read)
    connect(btnGetAll, &QPushButton::clicked, this, &MainWindow::onBtnGetAllParams);
}

// ==========================================================
// 逻辑实现
// ==========================================================

void MainWindow::refreshSerialPorts()
{
    comboPort->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        comboPort->addItem(info.portName());
    }
}

void MainWindow::toggleSerialPort()
{
    if (serial->isOpen()) {
        serial->close();
        btnOpenClose->setText(tr("Open Port"));
        comboPort->setEnabled(true);
        comboBaud->setEnabled(true);
        btnRefresh->setEnabled(true);
    } else {
        serial->setPortName(comboPort->currentText());
        serial->setBaudRate(comboBaud->currentText().toInt());
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);
        serial->setFlowControl(QSerialPort::NoFlowControl);

        if (serial->open(QIODevice::ReadWrite)) {
            btnOpenClose->setText(tr("Close Port"));
            // 只禁用配置，保留关闭按钮
            comboPort->setEnabled(false);
            comboBaud->setEnabled(false);
            btnRefresh->setEnabled(false);
        } else {
            QMessageBox::critical(this, tr("Error"), tr("Cannot open port: %1").arg(serial->errorString()));
        }
    }
}

void MainWindow::readSerialData()
{
    QByteArray data = serial->readAll();
    m_buffer.append(data);

    // 显示日志
    QString display;
    if (checkHexDisplay->isChecked()) {
        display = data.toHex(' ').toUpper();
    } else {
        display = QString::fromLocal8Bit(data);
    }

    textLog->moveCursor(QTextCursor::End);
    textLog->setTextColor(QColor("#2e7d32")); // Dark Green
    textLog->insertPlainText(display);
    textLog->moveCursor(QTextCursor::End);

    // 按行解析
    while (m_buffer.contains('\n')) {
        int lineEndIndex = m_buffer.indexOf('\n');
        QString line = QString::fromLocal8Bit(m_buffer.left(lineEndIndex)).trimmed();
        m_buffer.remove(0, lineEndIndex + 1);
        if (!line.isEmpty()) {
            parseLine(line);
        }
    }
}

void MainWindow::parseLine(const QString &line)
{
    // 1. 版本: AT+GETVER=software:v...,hardware:v...
    if (line.contains("GETVER=") || line.contains("software:")) {
        int idx = line.indexOf("=");
        QString val = (idx != -1) ? line.mid(idx + 1) : line;
        dispVersion->setText(val);
    }
    // 2. 配置: AT+GETCFG=0,1,1,1 (ID, Role, Rate, Filter)
    else if (line.contains("GETCFG=")) {
        QString params = line.section('=', 1);
        QStringList parts = params.split(',');
        if (parts.size() >= 4) {
            dispDevId->setText(parts[0]);

            int role = parts[1].toInt();
            dispRole->setText(role == 1 ? "Anchor (1)" : "Tag (0)");

            int rate = parts[2].toInt();
            dispRate->setText(rate == 1 ? "6.8Mbps" : "850kbps");

            int filter = parts[3].toInt();
            dispFilter->setText(filter == 1 ? "Enabled" : "Disabled");
        }
    }
    // 3. PAN: AT+GETPAN=1234
    else if (line.contains("GETPAN=") || (line.contains("PAN=") && !line.contains("SETPAN"))) {
        dispPanId->setText(line.section('=', 1).trimmed());
    }
    // 4. 天线: AT+GETANT=16536
    else if (line.contains("GETANT=")) {
        dispAntDelay->setText(line.section('=', 1).trimmed());
    }
    // 5. 功率: AT+GETPOW=FD
    else if (line.contains("GETPOW=")) {
        dispPower->setText(line.section('=', 1).trimmed());
    }
    // 6. 容量: AT+GETCAP=10,10,1
    else if (line.contains("GETCAP=")) {
        QString params = line.section('=', 1);
        QStringList parts = params.split(',');
        if (parts.size() >= 3) {
            QString mode = (parts[2].toInt() == 1) ? "Ext" : "Std";
            dispCapacity->setText(QString("Tag:%1 | Slot:%2ms | %3")
                                      .arg(parts[0], parts[1], mode));
        }
    }
    // 7. 上报: AT+GETRPT=1
    else if (line.contains("GETRPT=")) {
        int val = line.section('=', 1).toInt();
        dispRptStatus->setText(val == 1 ? "Enabled" : "Disabled");
    }
}

void MainWindow::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        QMessageBox::critical(this, tr("Error"), tr("Serial connection lost!"));
        if (serial->isOpen()) serial->close();
        btnOpenClose->setText(tr("Open Port"));
        comboPort->setEnabled(true);
        comboBaud->setEnabled(true);
        btnRefresh->setEnabled(true);
    }
}

void MainWindow::sendCommand(QString cmd)
{
    if (!serial->isOpen()) {
        QMessageBox::warning(this, tr("Warning"), tr("Please open serial port first!"));
        return;
    }
    QByteArray data = cmd.toLocal8Bit() + "\r\n";
    serial->write(data);

    textLog->moveCursor(QTextCursor::End);
    textLog->setTextColor(QColor("#1565c0")); // Dark Blue
    textLog->insertPlainText("TX: " + cmd + "\n");
    textLog->moveCursor(QTextCursor::End);
}

// --- 核心功能实现 ---

// 一键获取所有参数
void MainWindow::onBtnGetAllParams()
{
    if (!serial->isOpen()) {
        QMessageBox::warning(this, "Info", "Please open serial port");
        return;
    }
    // 依次发送所有查询指令
    // UWB模块通常处理速度很快，如果出现粘包，可以考虑增加延时，
    // 但根据手册AT机制，通常直接发送即可。
    sendCommand("AT+GETVER?");
    sendCommand("AT+GETCFG?");
    sendCommand("AT+GETPAN?");
    sendCommand("AT+GETANT?");
    sendCommand("AT+GETPOW?");
    sendCommand("AT+GETCAP?");
    sendCommand("AT+GETRPT?");
}

void MainWindow::onBtnSetCfg() {
    QString cmd = QString("AT+SETCFG=%1,%2,%3,%4")
    .arg(inputDevId->text())
        .arg(inputRole->currentData().toInt())
        .arg(inputRate->currentData().toInt())
        .arg(inputFilter->isChecked() ? 1 : 0);
    sendCommand(cmd);
    // 设置后建议自动查询一次以更新状态
    // QTimer::singleShot(200, this, [=](){ sendCommand("AT+GETCFG?"); });
}

void MainWindow::onBtnSetPan() {
    sendCommand(QString("AT+SETPAN=%1").arg(inputPanId->text()));
}

void MainWindow::onBtnSetAnt() {
    sendCommand(QString("AT+SETANT=%1").arg(inputAntDelay->text()));
}

void MainWindow::onBtnSetPow() {
    sendCommand(QString("AT+SETPOW=%1").arg(inputPower->text()));
}

void MainWindow::onBtnSetCap() {
    sendCommand(QString("AT+SETCAP=%1,%2,%3")
                    .arg(inputTagCount->text())
                    .arg(inputSlotTime->text())
                    .arg(inputExtMode->currentData().toInt()));
}

void MainWindow::onBtnSetRpt() {
    sendCommand(QString("AT+SETRPT=%1").arg(inputAutoRpt->isChecked() ? 1 : 0));
}

void MainWindow::onBtnSleep() {
    sendCommand(QString("AT+SLEEP=%1").arg(inputSleepTime->text()));
}

void MainWindow::onBtnSendData() {
    sendCommand(QString("AT+DATA=%1,%2")
                    .arg(inputDataLen->text())
                    .arg(inputDataContent->text()));
}

void MainWindow::onBtnCheckConn() { sendCommand("AT?"); }
void MainWindow::onBtnRestart() { sendCommand("AT+RESTART"); }
void MainWindow::onBtnRestore() {
    if(QMessageBox::question(this, "Confirm", "Are you sure you want to factory reser？") == QMessageBox::Yes)
        sendCommand("AT+RESTORE");
}
void MainWindow::onBtnSave() { sendCommand("AT+SAVE"); }
void MainWindow::onBtnClearLog() { textLog->clear(); }

#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <QtMath>
#include <QDateTime>
#include <QSplitter>
#include <QGroupBox>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QProcess>
#include <QFileDialog>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegExp> // Qt5.4 使用 QRegExp 处理正则比较方便
#include <QSet>
#include <algorithm> // 用于 std::sort 或 std::swap
#include <QScrollBar>

// ==========================================
// 调试宏定义
// ==========================================
// 注释掉下面这行即可关闭调试打印
//#define DEBUG_ANCHORS

// ==========================================
// MapWidget 实现
// ==========================================

MapWidget::MapWidget(QWidget *parent) : QWidget(parent), m_scale(1.0), m_offsetX(0), m_offsetY(0), m_margin(50.0)
{
    setMinimumSize(400, 400);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setAutoFillBackground(true);
    setPalette(pal);

    // 加载基站图片
    m_anchorImage.load(":/anchor.png");
}

void MapWidget::updateAnchors(const QVector<Point> &anchors)
{
    update();
}

void MapWidget::updateAnchorsMap(const QMap<int, Point> &anchorsMap)
{
    m_anchors = anchorsMap;
    update();
}

QMap<int, MapWidget::Point> MapWidget::getAnchorsMap() const
{
    return m_anchors;
}

void MapWidget::updateTag(int id, int x, int y)
{
    Tag tag;
    tag.pos.x = x;
    tag.pos.y = y;
    tag.color = Qt::red;
    m_tags[id] = tag;
    update();
}

void MapWidget::calculateTransform()
{
    double minX = 0, maxX = 100, minY = 0, maxY = 100;
    bool first = true;

    auto checkPoint = [&](double x, double y) {
        if (first) {
            minX = maxX = x;
            minY = maxY = y;
            first = false;
        } else {
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    };

    for (auto a : m_anchors) checkPoint(a.x, a.y);
    for (auto t : m_tags) checkPoint(t.pos.x, t.pos.y);

    double dataW = maxX - minX;
    double dataH = maxY - minY;

    if (dataW == 0) dataW = 100;
    if (dataH == 0) dataH = 100;

    double screenW = width() - 2 * m_margin;
    double screenH = height() - 2 * m_margin;

    double scaleX = screenW / dataW;
    double scaleY = screenH / dataH;
    m_scale = qMin(scaleX, scaleY);

    double centerX = (minX + maxX) / 2.0;
    double centerY = (minY + maxY) / 2.0;

    m_offsetX = width() / 2.0 - centerX * m_scale;
    m_offsetY = height() / 2.0 + centerY * m_scale;
}

QPointF MapWidget::worldToScreen(double wx, double wy)
{
    double sx = wx * m_scale + m_offsetX;
    double sy = m_offsetY - wy * m_scale;
    return QPointF(sx, sy);
}

void MapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    calculateTransform();
    drawGrid(painter);

    // 绘制基站
    for (auto it = m_anchors.begin(); it != m_anchors.end(); ++it) {
        QPointF sPos = worldToScreen(it.value().x, it.value().y);

        if (!m_anchorImage.isNull()) {
            int w = 16;
            int h = 16;
            QRect targetRect(sPos.x() - w/2, sPos.y() - h/2, w, h);
            painter.drawPixmap(targetRect, m_anchorImage);
        } else {
            painter.setBrush(Qt::black);
            painter.setPen(Qt::NoPen);
            painter.drawRect(QRectF(sPos.x() - 8, sPos.y() - 8, 16, 16));
        }

        painter.setPen(Qt::black);
        painter.drawText(sPos + QPointF(16, 5), QString("A%1").arg(it.key()));
    }

    // 绘制标签
    for (auto it = m_tags.begin(); it != m_tags.end(); ++it) {
        QPointF sPos = worldToScreen(it.value().pos.x, it.value().pos.y);

        painter.setBrush(it.value().color);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(sPos, 8, 8);

        painter.setPen(it.value().color);
        QString text = QString("T%1 (%2, %3)").arg(it.key()).arg((int)it.value().pos.x).arg((int)it.value().pos.y);
        painter.drawText(sPos + QPointF(12, 5), text);
    }
}

void MapWidget::drawGrid(QPainter &painter)
{
    // QPen pen(QColor(220, 220, 220));
    // pen.setStyle(Qt::DashLine);
    // painter.setPen(pen);

    // int step = 50;
    // for (int x = 0; x < width(); x += step)
    //     painter.drawLine(x, 0, x, height());
    // for (int y = 0; y < height(); y += step)
    //     painter.drawLine(0, y, width(), y);

    QPen originPen(QColor(200, 200, 200), 2);
    painter.setPen(originPen);
    QPointF origin = worldToScreen(0, 0);
    painter.drawLine(origin.x() - 20, origin.y(), origin.x() + 20, origin.y());
    painter.drawLine(origin.x(), origin.y() - 20, origin.x(), origin.y() + 20);
}


// ==========================================
// MainWindow 实现
// ==========================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_serial(new QSerialPort(this))
{
    setWindowTitle("UWB Positioning Tools");
    resize(1200, 800);

    m_settings = new QSettings("Makerfabs", "UWB_Qt_Cpp", this);

    initUI();
    loadSettings();

    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::onSerialReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &MainWindow::onSerialError);
}

MainWindow::~MainWindow()
{
    saveSettings();
    if (m_serial->isOpen())
        m_serial->close();
}

void MainWindow::initUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    m_mapWidget = new MapWidget(this);

    QFrame *controlPanel = new QFrame(this);
    controlPanel->setFixedWidth(360);
    controlPanel->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);

    // 1. 串口设置
    QGroupBox *gbSerial = new QGroupBox("Communication Settings", this);
    QVBoxLayout *vboxSerial = new QVBoxLayout(gbSerial);

    m_comboPorts = new QComboBox(this);
    QPushButton *btnRefresh = new QPushButton("Refresh Ports", this);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::refreshPorts);

    m_btnConnect = new QPushButton("Connect", this);
    m_btnConnect->setCheckable(true);
    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::toggleConnection);

    QPushButton *btnConfigTools = new QPushButton("Config Tools", this);
    connect(btnConfigTools, &QPushButton::clicked, this, &MainWindow::onOpenExternalApp);

    vboxSerial->addWidget(new QLabel("Port:", this));
    vboxSerial->addWidget(m_comboPorts);
    vboxSerial->addWidget(btnRefresh);
    vboxSerial->addWidget(m_btnConnect);
    vboxSerial->addWidget(btnConfigTools);

    // 2. 基站配置 (完全动态化)
    QGroupBox *gbAnchors = new QGroupBox("Anchor Configuration (ID | X | Y)", this);
    QVBoxLayout *vboxAnchors = new QVBoxLayout(gbAnchors);

    m_tableAnchors = new QTableWidget(0, 3, this);
    m_tableAnchors->setHorizontalHeaderLabels({"ID", "X (cm)", "Y (cm)"});
    m_tableAnchors->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableAnchors->setAlternatingRowColors(true);

    QHBoxLayout *hboxTableTools = new QHBoxLayout();
    QPushButton *btnAddRow = new QPushButton("Add Anchor", this);
    QPushButton *btnDelRow = new QPushButton("Delete Selected", this);

    connect(btnAddRow, &QPushButton::clicked, this, [=](){
        int row = m_tableAnchors->rowCount();
        m_tableAnchors->insertRow(row);
        m_tableAnchors->setItem(row, 0, new QTableWidgetItem(QString::number(row))); // ID
        m_tableAnchors->setItem(row, 1, new QTableWidgetItem("0")); // X
        m_tableAnchors->setItem(row, 2, new QTableWidgetItem("0")); // Y
    });

    connect(btnDelRow, &QPushButton::clicked, this, [=](){
        QList<QTableWidgetItem*> selection = m_tableAnchors->selectedItems();
        QSet<int> rows;
        for(auto item : selection) rows.insert(item->row());
        QList<int> sortedRows = rows.values();
        std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
        for(int r : sortedRows) m_tableAnchors->removeRow(r);
    });

    hboxTableTools->addWidget(btnAddRow);
    hboxTableTools->addWidget(btnDelRow);

    QPushButton *btnApply = new QPushButton("Apply and Reset Map", this);
    connect(btnApply, &QPushButton::clicked, this, &MainWindow::applyAnchors);

    vboxAnchors->addWidget(m_tableAnchors);
    vboxAnchors->addLayout(hboxTableTools);
    vboxAnchors->addWidget(btnApply);

    // 3. 算法设置
    QGroupBox *gbAlgorithm = new QGroupBox("Optimization", this);
    QVBoxLayout *vboxAlgo = new QVBoxLayout(gbAlgorithm);

    QHBoxLayout *hboxThreshold = new QHBoxLayout();
    hboxThreshold->addWidget(new QLabel("Fluctuation Threshold (cm):"));
    m_spinThreshold = new QDoubleSpinBox(this);
    m_spinThreshold->setRange(0, 100);
    m_spinThreshold->setValue(10.0);
    hboxThreshold->addWidget(m_spinThreshold);

    vboxAlgo->addLayout(hboxThreshold);
    gbAlgorithm->setLayout(vboxAlgo);

    // 4. 日志区域 (Log Output) - 替换了旧的显示区域
    QGroupBox *gbLog = new QGroupBox("System Log", this);
    QVBoxLayout *vboxLog = new QVBoxLayout(gbLog);

    m_lblConnection = new QLabel("Ready", this);
    m_lblConnection->setAlignment(Qt::AlignCenter);
    m_lblConnection->setStyleSheet("background-color: #eee; padding: 5px; border-radius: 4px;");
    vboxLog->addWidget(m_lblConnection);

    m_txtLog = new QTextEdit(this);
    m_txtLog->setReadOnly(true);
    // 设置等宽字体，看起来更像终端日志
    QFont logFont("Consolas");
    logFont.setStyleHint(QFont::Monospace);
    m_txtLog->setFont(logFont);
    vboxLog->addWidget(m_txtLog);

    gbLog->setLayout(vboxLog);

    controlLayout->addWidget(gbSerial);
    controlLayout->addWidget(gbAnchors);
    controlLayout->addWidget(gbAlgorithm);
    controlLayout->addWidget(gbLog, 1); // Log 区域占据剩余空间

    mainLayout->addWidget(m_mapWidget, 1);
    mainLayout->addWidget(controlPanel);

    refreshPorts();
}

void MainWindow::onOpenExternalApp()
{
    // 获取当前 A 程序所在的目录
    QString currentDir = QCoreApplication::applicationDirPath();

    QString appFileName = "uwbtools.exe";
    QString appPath = currentDir + "/" + appFileName;

    QFileInfo bInfo(appPath);

    // 1. 检查文件是否存在
    if (!bInfo.exists() || !bInfo.isFile()) {
        QMessageBox::warning(this, "Error", "Could not find Application B (" + appFileName + ") in:\n" + currentDir);
        return;
    }

    bool success = QProcess::startDetached(appPath, QStringList(), currentDir);

    if (!success) {
        QMessageBox::critical(this, "Error", "Failed to start Application B.\nPath: " + appPath);
    }
}

void MainWindow::refreshPorts()
{
    m_comboPorts->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        m_comboPorts->addItem(info.portName());
    }
}

void MainWindow::toggleConnection()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        m_btnConnect->setText("Connect");
        m_comboPorts->setEnabled(true);
        m_lblConnection->setText("Disconnected");
        m_lblConnection->setStyleSheet("background-color: #fdd; color: red; padding: 5px; border-radius: 4px;");
        logMessage("System: Port closed.");
    } else {
        QString portName = m_comboPorts->currentText();
        if (portName.isEmpty()) return;

        m_serial->setPortName(portName);
        m_serial->setBaudRate(115200);

        if (m_serial->open(QIODevice::ReadWrite)) {
            m_btnConnect->setText("Disconnect");
            m_comboPorts->setEnabled(false);
            m_lblConnection->setText("Connected: " + portName);
            m_lblConnection->setStyleSheet("background-color: #dfd; color: green; padding: 5px; border-radius: 4px;");

            m_serial->write("begin");
            m_serialBuffer.clear();
            m_txtLog->clear();
            logMessage("System: Port opened successfully.");
        } else {
            QMessageBox::critical(this, "Error", "Cannot open serial port: " + m_serial->errorString());
            m_btnConnect->setChecked(false);
        }
    }
}


void MainWindow::logMessage(const QString &msg)
{
    QString timeStr = QDateTime::currentDateTime().toString("[HH:mm:ss.zzz] ");
    m_txtLog->append(timeStr + msg);

    // 自动滚动到底部
    QScrollBar *sb = m_txtLog->verticalScrollBar();
    sb->setValue(sb->maximum());

    // 限制行数防止内存无限增长
    if (m_txtLog->document()->blockCount() > 500) {
        m_txtLog->clear();
        m_txtLog->append("[System] Log cleared (buffer full)");
    }
}

void MainWindow::loadSettings()
{
    QString lastPort = m_settings->value("lastPort").toString();
    int idx = m_comboPorts->findText(lastPort);
    if (idx >= 0) m_comboPorts->setCurrentIndex(idx);

    double threshold = m_settings->value("distThreshold", 10.0).toDouble();
    m_spinThreshold->setValue(threshold);

    // 加载基站列表
    int count = m_settings->beginReadArray("Anchors");
    if (count == 0) {
        m_tableAnchors->setRowCount(4);
        m_tableAnchors->setItem(0, 0, new QTableWidgetItem("0")); m_tableAnchors->setItem(0, 1, new QTableWidgetItem("0")); m_tableAnchors->setItem(0, 2, new QTableWidgetItem("0"));
        m_tableAnchors->setItem(1, 0, new QTableWidgetItem("1")); m_tableAnchors->setItem(1, 1, new QTableWidgetItem("130")); m_tableAnchors->setItem(1, 2, new QTableWidgetItem("0"));
        m_tableAnchors->setItem(2, 0, new QTableWidgetItem("2")); m_tableAnchors->setItem(2, 1, new QTableWidgetItem("130")); m_tableAnchors->setItem(2, 2, new QTableWidgetItem("130"));
        m_tableAnchors->setItem(3, 0, new QTableWidgetItem("3")); m_tableAnchors->setItem(3, 1, new QTableWidgetItem("0")); m_tableAnchors->setItem(3, 2, new QTableWidgetItem("130"));
    } else {
        m_tableAnchors->setRowCount(count);
        for (int i = 0; i < count; ++i) {
            m_settings->setArrayIndex(i);
            m_tableAnchors->setItem(i, 0, new QTableWidgetItem(m_settings->value("id").toString()));
            m_tableAnchors->setItem(i, 1, new QTableWidgetItem(m_settings->value("x").toString()));
            m_tableAnchors->setItem(i, 2, new QTableWidgetItem(m_settings->value("y").toString()));
        }
    }
    m_settings->endArray();

    applyAnchors();
}

void MainWindow::saveSettings()
{
    m_settings->setValue("lastPort", m_comboPorts->currentText());
    m_settings->setValue("distThreshold", m_spinThreshold->value());

    // 保存基站列表
    m_settings->beginWriteArray("Anchors");
    for (int i = 0; i < m_tableAnchors->rowCount(); ++i) {
        m_settings->setArrayIndex(i);
        auto itemID = m_tableAnchors->item(i, 0);
        auto itemX = m_tableAnchors->item(i, 1);
        auto itemY = m_tableAnchors->item(i, 2);

        if (itemID && itemX && itemY) {
            m_settings->setValue("id", itemID->text());
            m_settings->setValue("x", itemX->text());
            m_settings->setValue("y", itemY->text());
        }
    }
    m_settings->endArray();
}

void MainWindow::applyAnchors()
{
    QMap<int, MapWidget::Point> anchorsMap;

    for (int i = 0; i < m_tableAnchors->rowCount(); ++i) {
        auto itemID = m_tableAnchors->item(i, 0);
        auto itemX = m_tableAnchors->item(i, 1);
        auto itemY = m_tableAnchors->item(i, 2);

        if (itemID && itemX && itemY) {
            int id = itemID->text().toInt();
            int x = itemX->text().toInt();
            int y = itemY->text().toInt();
            anchorsMap[id] = {x, y};
        }
    }

    m_mapWidget->updateAnchorsMap(anchorsMap);
    saveSettings();
}

void MainWindow::onSerialReadyRead()
{
    m_serialBuffer.append(m_serial->readAll());

    while (m_serialBuffer.contains('\n')) {
        int lineEnd = m_serialBuffer.indexOf('\n');
        QByteArray line = m_serialBuffer.left(lineEnd).trimmed();
        m_serialBuffer.remove(0, lineEnd + 1);

        if (!line.isEmpty()) {
            processData(line);
        }
    }
}

// --------------------------------------------------------
// 新的解析逻辑：处理 AT 指令格式
// 示例格式: AT+RANGE=tid:1,mask:80,seq:65,range:(0,0,0,0,0,0,0,107),ancid:(-1,-1,-1,-1,-1,-1,-1,7)
// --------------------------------------------------------
void MainWindow::processData(const QByteArray &data)
{
    QString strData = QString::fromLatin1(data).trimmed();
//    qDebug() << strData;
    if (!strData.startsWith("AT+RANGE=")) return;

    // 解析 tid
    int tagId = -1;
    QRegExp rxId("tid:(\\d+)");
    if (rxId.indexIn(strData) != -1) {
        tagId = rxId.cap(1).toInt();
    } else {
        return;
    }

    // 解析 range 和 ancid
    QVector<int> rawRanges;
    QVector<int> aidArr;

    QRegExp rxRange("range:\\(([^)]+)\\)");
    if (rxRange.indexIn(strData) != -1) {
        QString rangeContent = rxRange.cap(1);
        QStringList parts = rangeContent.split(',');
        for (const QString &val : parts)
            if (val.toInt()) rawRanges.append(val.toInt());
    }

    QRegExp rxAncid("ancid:\\(([^)]+)\\)");
    if (rxAncid.indexIn(strData) != -1) {
        QString ancidContent = rxAncid.cap(1);
        QStringList parts = ancidContent.split(',');
        for (const QString &val : parts)
            if (val.toInt() + 1) aidArr.append(val.toInt());
    }

    // 如果没有数据或长度不匹配，退出
    if ((aidArr.size() != rawRanges.size()) || (aidArr.size() < 3)) {
        qDebug() << "value data < 3";
        return;
    }


    #ifdef DEBUG_ANCHORS
    qDebug() << "------------------------------------------------";
    qDebug() << "Parsed Data -> Tag:" << tagId;
    qDebug() << "Raw Ranges:" << rawRanges;
    qDebug() << "Anc IDs:" << aidArr;

    QString validStr;
    for(auto r : currentRanges) validStr += QString("[%1: %2cm] ").arg(r.aid).arg(r.dist);
    qDebug() << "Valid Pairs (Dist>0):" << validStr;

    QString knownStr;
    for(auto k : knownAnchors.keys()) knownStr += QString("%1 ").arg(k);
    qDebug() << "Configured Anchors in UI:" << knownStr;
    #endif

    // 匹配有效基站
    QMap<int, MapWidget::Point> mapAnchors = m_mapWidget->getAnchorsMap();
    QVector<MapWidget::Point> validPoints;
    QVector<int> validRanges;
    QString usedAnchorsStr;

    for (int i = 0; i < aidArr.size(); i++) {
        int aid = aidArr[i];
        if (mapAnchors.contains(aid)) {
            validPoints.append(mapAnchors[aid]);
            validRanges.append(rawRanges[i]);
            usedAnchorsStr += QString("A%1:%2 ").arg(aid).arg(rawRanges[i]);
        }
    }

    if (validPoints.size() < 3) {
        logMessage(QString("Tag %1: Not enough known anchors (%2 found)").arg(tagId).arg(validPoints.size()));
        return;
    }

    QPoint rawPos;
    if (calculatePosition(validPoints, validRanges, rawPos)) {

        // 简单的平滑滤波
        QPoint finalPos = rawPos;
        if (m_lastTagPoint.contains(tagId)) {
            QPoint last = m_lastTagPoint[tagId];
            double alpha = 0.4; // 滤波系数
            finalPos.setX(last.x() * (1-alpha) + rawPos.x() * alpha);
            finalPos.setY(last.y() * (1-alpha) + rawPos.y() * alpha);

            // 静止过滤
            if ((finalPos - last).manhattanLength() < m_spinThreshold->value()) {
                finalPos = last;
            }
        }

        m_lastTagPoint[tagId] = finalPos;
        m_mapWidget->updateTag(tagId, finalPos.x(), finalPos.y());

        // 输出计算日志
        logMessage(QString("Tag %1 -> (%2, %3) | Used: %4")
                   .arg(tagId).arg(finalPos.x()).arg(finalPos.y()).arg(usedAnchorsStr));
    } else {
        logMessage(QString("Tag %1: Calc Failed").arg(tagId));
    }
}


void MainWindow::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        QMessageBox::critical(this, "Connection Lost", "Serial device removed");
        toggleConnection();
    }
}

//bool MainWindow::calculatePosition(const QVector<MapWidget::Point> &anchors, const QVector<int> &ranges, QPoint &result)
//{
//    int n = qMin(anchors.size(), ranges.size());
//    if (n < 3) return false;

//    struct ValidData { int x, y, r; };
//    QVector<ValidData> data;
//    for(int i=0; i<n; ++i) {
//        if(ranges[i] > 0) data.append({anchors[i].x, anchors[i].y, ranges[i]});
//    }

//    if(data.size() < 3) return false;

//    double x1 = data[0].x, y1 = data[0].y, r1 = data[0].r;
//    double x2 = data[1].x, y2 = data[1].y, r2 = data[1].r;
//    double x3 = data[2].x, y3 = data[2].y, r3 = data[2].r;

//    // 使用 double 避免大距离时的 int 溢出
//    double A = 2.0 * (x2 - x1);
//    double B = 2.0 * (y2 - y1);
//    double C = r1*r1 - r2*r2 - x1*x1 + x2*x2 - y1*y1 + y2*y2;

//    double D = 2.0 * (x3 - x2);
//    double E = 2.0 * (y3 - y2);
//    double F = r2*r2 - r3*r3 - x2*x2 + x3*x3 - y2*y2 + y3*y3;

//    double det = A * E - B * D;

//    if (qAbs(det) < 1e-4) {
//        qDebug() << "Math Error: Anchors are collinear (Singular Matrix)";
//        return false;
//    }

//    double x = (C * E - B * F) / det;
//    double y = (A * F - C * D) / det;

//    result = QPoint(qRound(x), qRound(y));
//    return true;
//}

// --------------------------------------------------------
// 核心算法：线性化最小二乘法 (Linearized Least Squares)
// 优化：自动选取距离最近的基站作为参考点，减少误差扩散
// --------------------------------------------------------
bool MainWindow::calculatePosition(const QVector<MapWidget::Point> &anchors, const QVector<int> &ranges, QPoint &result)
{
    int n = qMin(anchors.size(), ranges.size());
    if (n < 3) return false;

    // 1. 数据预处理与优化：寻找距离最近的基站
    // 为什么要找最近的？
    // 因为算法需要选一个点做减法参考。距离越近，通常信号质量越好（Line of Sight），
    // 把它作为参考点，可以防止把远距离基站的大误差引入到所有方程中。

    QVector<double> X(n), Y(n), R(n);
    int bestIdx = 0;
    int minRange = 999999;

    for(int i = 0; i < n; ++i) {
        X[i] = anchors[i].x;
        Y[i] = anchors[i].y;
        R[i] = ranges[i];
        if (ranges[i] < minRange) {
            minRange = ranges[i];
            bestIdx = i;
        }
    }

    // 将最佳基站交换到数组末尾，作为参考点 (Xn, Yn)
    if (bestIdx != n - 1) {
        std::swap(X[bestIdx], X[n-1]);
        std::swap(Y[bestIdx], Y[n-1]);
        std::swap(R[bestIdx], R[n-1]);
    }

    // 参考点
    double xn = X[n-1];
    double yn = Y[n-1];
    double rn = R[n-1];

    // 构建矩阵 A 和向量 b (A*x = b)
    // 最小二乘解公式: x = (A^T * A)^-1 * (A^T * b)
    // 这里我们手动展开矩阵乘法，避免引入复杂的矩阵库

    double a11 = 0, a12 = 0, a22 = 0; // A^T * A 的元素 (a21 = a12)
    double b1 = 0, b2 = 0;           // A^T * b 的元素

    for (int i = 0; i < n - 1; ++i) {
        // 原始方程: (x-xi)^2 + (y-yi)^2 = ri^2
        // 线性化后: 2x(xi - xn) + 2y(yi - yn) = ri^2 - rn^2 - xi^2 + xn^2 - yi^2 + yn^2

        double Ai_0 = 2.0 * (X[i] - xn);
        double Ai_1 = 2.0 * (Y[i] - yn);

        double bi_val = rn*rn - R[i]*R[i] + X[i]*X[i] - xn*xn + Y[i]*Y[i] - yn*yn;

        // 累加 A^T * A
        a11 += Ai_0 * Ai_0;
        a12 += Ai_0 * Ai_1;
        a22 += Ai_1 * Ai_1;

        // 累加 A^T * b
        b1 += Ai_0 * bi_val;
        b2 += Ai_1 * bi_val;
    }

    // 计算行列式 det
    double det = a11 * a22 - a12 * a12;

    // 如果行列式接近0，说明基站共线，无解
    if (qAbs(det) < 1e-4) {
        return false;
    }

    // 求解
    double x = (a22 * b1 - a12 * b2) / det;
    double y = (a11 * b2 - a12 * b1) / det;

    result = QPoint(qRound(x), qRound(y));
    return true;
}

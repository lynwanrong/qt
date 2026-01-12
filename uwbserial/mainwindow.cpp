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
#include <QRegExp>
#include <QSet>
#include <algorithm>
#include <QScrollBar>

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

    // load anchor png
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
        painter.drawEllipse(sPos, 6, 6);

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

    // 1. serial set
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

    // 2. anchor config
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

    // 3. Algorithm set
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

    // 4. Log Output
    QGroupBox *gbLog = new QGroupBox("System Log", this);
    QVBoxLayout *vboxLog = new QVBoxLayout(gbLog);

    m_lblConnection = new QLabel("Ready", this);
    m_lblConnection->setAlignment(Qt::AlignCenter);
    m_lblConnection->setStyleSheet("background-color: #eee; padding: 5px; border-radius: 4px;");
    vboxLog->addWidget(m_lblConnection);

    m_txtLog = new QTextEdit(this);
    m_txtLog->setReadOnly(true);

    QFont logFont("Consolas");
    logFont.setStyleHint(QFont::Monospace);
    m_txtLog->setFont(logFont);
    vboxLog->addWidget(m_txtLog);

    gbLog->setLayout(vboxLog);

    controlLayout->addWidget(gbSerial);
    controlLayout->addWidget(gbAnchors);
    controlLayout->addWidget(gbAlgorithm);
    controlLayout->addWidget(gbLog, 1);

    mainLayout->addWidget(m_mapWidget, 1);
    mainLayout->addWidget(controlPanel);

    refreshPorts();
}

void MainWindow::onOpenExternalApp()
{
    QString currentDir = QCoreApplication::applicationDirPath();

    QString appFileName = "uwbtools.exe";
    QString appPath = currentDir + "/" + appFileName;

    QFileInfo bInfo(appPath);

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

    QScrollBar *sb = m_txtLog->verticalScrollBar();
    sb->setValue(sb->maximum());

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

    // load anchor tables
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

    // save anchor tables
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
// 示例格式: AT+RANGE=tid:1,mask:80,seq:65,range:(0,0,0,0,0,0,0,107),ancid:(-1,-1,-1,-1,-1,-1,-1,7)
// --------------------------------------------------------
void MainWindow::processData(const QByteArray &data)
{
    QString strData = QString::fromLatin1(data).trimmed();
//    qDebug() << strData;
    if (!strData.startsWith("AT+RANGE=")) return;

    // parse tid
    int tagId = -1;
    QRegExp rxId("tid:(\\d+)");
    if (rxId.indexIn(strData) != -1) {
        tagId = rxId.cap(1).toInt();
    } else {
        return;
    }

    // parse range and ancid
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

    //
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

        QPoint finalPos = rawPos;
        if (m_lastTagPoint.contains(tagId)) {
            QPoint last = m_lastTagPoint[tagId];
            double alpha = 0.2; // 滤波系数
            finalPos.setX(last.x() * (1-alpha) + rawPos.x() * alpha);
            finalPos.setY(last.y() * (1-alpha) + rawPos.y() * alpha);

            if ((finalPos - last).manhattanLength() < m_spinThreshold->value()) {
                finalPos = last;
            }
        }

        m_lastTagPoint[tagId] = finalPos;
        m_mapWidget->updateTag(tagId, finalPos.x(), finalPos.y());

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



bool MainWindow::calculatePosition(const QVector<MapWidget::Point> &anchors, const QVector<int> &ranges, QPoint &result)
{
    int n = qMin(anchors.size(), ranges.size());
    if (n < 3) return false;

    QVector<double> X(n), Y(n), R(n);
    int bestIdx = 0;
    int minRange = 999999;

    for(int i = 0; i < n; ++i) {
        X[i] = anchors[i].x;
        Y[i] = anchors[i].y;
        R[i] = ranges[i];
        if (ranges[i] < minRange && ranges[i] > 0) {
            minRange = ranges[i];
            bestIdx = i;
        }
    }

    if (bestIdx != n - 1) {
        std::swap(X[bestIdx], X[n-1]);
        std::swap(Y[bestIdx], Y[n-1]);
        std::swap(R[bestIdx], R[n-1]);
    }

    double xn = X[n-1], yn = Y[n-1], rn = R[n-1];
    double a11 = 0, a12 = 0, a22 = 0, b1 = 0, b2 = 0;

    for (int i = 0; i < n - 1; ++i) {
        double Ai_0 = 2.0 * (X[i] - xn);
        double Ai_1 = 2.0 * (Y[i] - yn);
        double bi_val = rn*rn - R[i]*R[i] + X[i]*X[i] - xn*xn + Y[i]*Y[i] - yn*yn;

        a11 += Ai_0 * Ai_0;
        a12 += Ai_0 * Ai_1;
        a22 += Ai_1 * Ai_1;
        b1 += Ai_0 * bi_val;
        b2 += Ai_1 * bi_val;
    }

    double det = a11 * a22 - a12 * a12;
    if (qAbs(det) < 1e-4) return false;

    double x = (a22 * b1 - a12 * b2) / det;
    double y = (a11 * b2 - a12 * b1) / det;

    result = QPoint(qRound(x), qRound(y));
    return true;
}

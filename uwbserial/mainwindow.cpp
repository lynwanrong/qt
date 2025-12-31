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

void MapWidget::updateTag(int id, double x, double y)
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
            int w = 32;
            int h = 32;
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
    QPen pen(QColor(220, 220, 220));
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);

    int step = 50;
    for (int x = 0; x < width(); x += step)
        painter.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += step)
        painter.drawLine(0, y, width(), y);

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

    // 4. 状态
    QGroupBox *gbStatus = new QGroupBox("Status", this);
    QVBoxLayout *vboxStatus = new QVBoxLayout(gbStatus);

    m_lblConnection = new QLabel("Ready", this);
    m_lblConnection->setStyleSheet("font-weight: bold; color: #333;");
    vboxStatus->addWidget(m_lblConnection);

    m_txtTagInfo = new QTextEdit(this);
    m_txtTagInfo->setReadOnly(true);
    vboxStatus->addWidget(m_txtTagInfo);

    gbStatus->setLayout(vboxStatus);

    controlLayout->addWidget(gbSerial);
    controlLayout->addWidget(gbAnchors);
    controlLayout->addWidget(gbAlgorithm);
    controlLayout->addWidget(gbStatus);

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
        m_lblConnection->setStyleSheet("color: red; font-weight: bold;");

        m_lastTagRanges.clear();
        m_tagInfoMap.clear();
        m_txtTagInfo->clear();
    } else {
        QString portName = m_comboPorts->currentText();
        if (portName.isEmpty()) return;

        m_serial->setPortName(portName);
        m_serial->setBaudRate(115200);

        if (m_serial->open(QIODevice::ReadWrite)) {
            m_btnConnect->setText("Disconnected");
            m_comboPorts->setEnabled(false);
            m_lblConnection->setText("Connected: " + portName);
            m_lblConnection->setStyleSheet("color: green; font-weight: bold;");

            m_serial->write("begin");
            m_serialBuffer.clear();
            m_lastTagRanges.clear();
            m_tagInfoMap.clear();
            m_txtTagInfo->clear();
        } else {
            QMessageBox::critical(this, "Error", "Cannot open serial port: " + m_serial->errorString());
            m_btnConnect->setChecked(false);
        }
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
            double x = itemX->text().toDouble();
            double y = itemY->text().toDouble();
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
//            processJsonData(line);
            processData(line);
        }
    }
}

void MainWindow::processJsonData(const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) return;

    if (doc.isObject()) {
        QJsonObject obj = doc.object();

        if (obj.contains("id")) {
            int tagId = obj["id"].toInt();

            // 准备定位数据
            struct RangeData { int aid; double dist; };
            QVector<RangeData> currentRanges;

            // === 修改：解析双数组格式 {"id":1,"range":[...],"aid":[...]} ===
            if (obj.contains("range") && obj.contains("aid")) {
                QJsonArray rangeArr = obj["range"].toArray();
                QJsonArray aidArr = obj["aid"].toArray();

                // 将原始range读入用于滤波
                QVector<double> rawRanges;
                for(auto v : rangeArr) rawRanges.append(v.toDouble());

                // === 滤波处理逻辑 (基于索引位置) ===
                QVector<double> filteredRanges;
                double threshold = m_spinThreshold->value();

                if (!m_lastTagRanges.contains(tagId) || m_lastTagRanges[tagId].size() != rawRanges.size()) {
                    m_lastTagRanges[tagId] = rawRanges;
                    filteredRanges = rawRanges;
                } else {
                    QVector<double> &lastRanges = m_lastTagRanges[tagId];
                    filteredRanges.resize(rawRanges.size());

                    for (int i = 0; i < rawRanges.size(); ++i) {
                        double newVal = rawRanges[i];
                        double oldVal = lastRanges[i];

                        // 滤波规则：如果在阈值内波动且不为0，保持旧值
                        if (newVal == 0) {
                            filteredRanges[i] = 0;
                        } else if (oldVal == 0) {
                            filteredRanges[i] = newVal;
                            lastRanges[i] = newVal;
                        } else if (qAbs(newVal - oldVal) <= threshold) {
                            filteredRanges[i] = oldVal;
                        } else {
                            filteredRanges[i] = newVal;
                            lastRanges[i] = newVal;
                        }
                    }
                }

                // === 组合数据：距离 > 0 则去查 aid 数组对应的 ID ===
                int count = qMin(filteredRanges.size(), aidArr.size());
                for(int i = 0; i < count; ++i) {
                    double dist = filteredRanges[i];
                    if (dist > 0) {
                        int aid = aidArr[i].toInt();
                        currentRanges.append({aid, dist});
                    }
                }
            }

            if (currentRanges.isEmpty()) return;

            // === 匹配已知基站坐标 ===
            // 从 UI 表格临时获取坐标 (或者如果 MapWidget 有接口更好)
            QMap<int, MapWidget::Point> knownAnchors;
            for (int i = 0; i < m_tableAnchors->rowCount(); ++i) {
                if(auto idItem = m_tableAnchors->item(i, 0)) {
                    int id = idItem->text().toInt();
                    double x = m_tableAnchors->item(i, 1)->text().toDouble();
                    double y = m_tableAnchors->item(i, 2)->text().toDouble();
                    knownAnchors[id] = {x, y};
                }
            }

            QVector<MapWidget::Point> validPoints;
            QVector<double> validDistances;
            QVector<int> debugIDs; // 用于调试打印的ID列表

            for(auto r : currentRanges) {
                if(knownAnchors.contains(r.aid)) {
                    validPoints.append(knownAnchors[r.aid]);
                    validDistances.append(r.dist);
                    debugIDs.append(r.aid);
                }
            }

            // === 调试宏打印 ===
            #ifdef DEBUG_ANCHORS
            if (!validPoints.isEmpty()) {
                 qDebug() << "=== Tag" << tagId << "Calculation Frame ===";
                 for(int i=0; i<validPoints.size(); ++i) {
                     qDebug() << "Anchor ID:" << debugIDs[i]
                              << " Pos:(" << validPoints[i].x << "," << validPoints[i].y << ")"
                              << " Dist:" << validDistances[i];
                 }
            }
            #endif

            // 计算位置
            QPointF pos;
            QString statusStr;

            if (calculatePosition(validPoints, validDistances, pos)) {
                m_mapWidget->updateTag(tagId, pos.x(), pos.y());
                statusStr = QString("Tag %1: (%2, %3) [Anchors:%4]").arg(tagId).arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1).arg(validPoints.size());
            } else {
                statusStr = QString("Tag %1: Positioning Failed (Valid Anchors<3)").arg(tagId);
            }

            m_tagInfoMap[tagId] = statusStr;
            updateTagStatusDisplay();
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

    // 1. 简单校验
    if (!strData.startsWith("AT+RANGE=")) return;

    // 2. 解析 tid
    int tagId = -1;
    // 使用 QRegExp (Qt5) 提取 tid: 后面的数字
    QRegExp rxId("tid:(\\d+)");
    if (rxId.indexIn(strData) != -1) {
        tagId = rxId.cap(1).toInt();
    } else {
        return; // 解析不到 ID
    }

    // 3. 提取 range 和 ancid 内容
    // 格式是 range:(1,2,3) 和 ancid:(1,2,3)
    QVector<double> rawRanges;
    QVector<int> aidArr;

    // 解析 Range
    QRegExp rxRange("range:\\(([^)]+)\\)");
    if (rxRange.indexIn(strData) != -1) {
        QString rangeContent = rxRange.cap(1);
        QStringList parts = rangeContent.split(',');
        for (const QString &val : parts) {
            rawRanges.append(val.toDouble());
        }
    }

    // 解析 Ancid (注意：这里在新的格式叫 ancid, 之前叫 aid)
    QRegExp rxAncid("ancid:\\(([^)]+)\\)");
    if (rxAncid.indexIn(strData) != -1) {
        QString ancidContent = rxAncid.cap(1);
        QStringList parts = ancidContent.split(',');
        for (const QString &val : parts) {
            aidArr.append(val.toInt());
        }
    }

    // 如果没有数据或长度不匹配，退出
    if (rawRanges.isEmpty() || aidArr.isEmpty()) return;

    // ==========================================
    // 以下逻辑复用之前的滤波和定位算法
    // ==========================================

    // 准备定位数据
    struct RangeData { int aid; double dist; };
    QVector<RangeData> currentRanges;

    // === 滤波处理逻辑 ===
    QVector<double> filteredRanges;
    double threshold = m_spinThreshold->value();

    if (!m_lastTagRanges.contains(tagId) || m_lastTagRanges[tagId].size() != rawRanges.size()) {
        m_lastTagRanges[tagId] = rawRanges;
        filteredRanges = rawRanges;
    } else {
        QVector<double> &lastRanges = m_lastTagRanges[tagId];
        filteredRanges.resize(rawRanges.size());

        for (int i = 0; i < rawRanges.size(); ++i) {
            double newVal = rawRanges[i];
            double oldVal = lastRanges[i];

            // 滤波规则：如果在阈值内波动且不为0，保持旧值
            if (newVal == 0) {
                filteredRanges[i] = 0;
            } else if (oldVal == 0) {
                filteredRanges[i] = newVal;
                lastRanges[i] = newVal;
            } else if (qAbs(newVal - oldVal) <= threshold) {
                filteredRanges[i] = oldVal;
            } else {
                filteredRanges[i] = newVal;
                lastRanges[i] = newVal;
            }
        }
    }

    // === 组合数据：距离 > 0 则去查 aid 数组对应的 ID ===
    int count = qMin(filteredRanges.size(), aidArr.size());
    for(int i = 0; i < count; ++i) {
        double dist = filteredRanges[i];
        if (dist > 0) {
            int aid = aidArr[i];
            currentRanges.append({aid, dist});
        }
    }

//    if (currentRanges.isEmpty()) return;

    // === 匹配已知基站坐标 ===
    QMap<int, MapWidget::Point> knownAnchors;
    for (int i = 0; i < m_tableAnchors->rowCount(); ++i) {
        if(auto idItem = m_tableAnchors->item(i, 0)) {
            int id = idItem->text().toInt();
            double x = m_tableAnchors->item(i, 1)->text().toDouble();
            double y = m_tableAnchors->item(i, 2)->text().toDouble();
            knownAnchors[id] = {x, y};
        }
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

//    if (currentRanges.isEmpty()) return;

    QVector<MapWidget::Point> validPoints;
    QVector<double> validDistances;
//    QVector<int> debugIDs;
    QSet<int> usedIDs;

    for(auto r : currentRanges) {
        if(knownAnchors.contains(r.aid)) {
            validPoints.append(knownAnchors[r.aid]);
            validDistances.append(r.dist);
//            debugIDs.append(r.aid);
            if (usedIDs.size() < 3) {
                usedIDs.insert(r.aid);
            }
        }
    }

    // === 调试宏打印 ===
//    #ifdef DEBUG_ANCHORS
//    if (!validPoints.isEmpty()) {
//         qDebug() << "=== Tag" << tagId << "Calculation Frame ===";
//         for(int i=0; i<validPoints.size(); ++i) {
//             qDebug() << "Anchor ID:" << debugIDs[i]
//                      << " Pos:(" << validPoints[i].x << "," << validPoints[i].y << ")"
//                      << " Dist:" << validDistances[i];
//         }
//    }
//    #endif

    // === 刷新表格 UI：高亮参与计算的基站ID ===
    // 遍历表格的所有行，如果 ID 在 usedIDs 中，则设为红色
    for(int i = 0; i < m_tableAnchors->rowCount(); ++i) {
        QTableWidgetItem *item = m_tableAnchors->item(i, 0); // 第0列是 ID 列
        if(!item) continue;

        int id = item->text().toInt();
        if(usedIDs.contains(id)) {
            // 设置红色背景，白色文字，表示该基站“中选”
            item->setBackground(Qt::red);
            item->setForeground(Qt::white);
        } else {
            // 恢复默认背景 (使用 QBrush() 恢复默认，保留 setAlternatingRowColors 的效果)
            item->setBackground(QBrush());
            item->setForeground(Qt::black);
        }
    }

    if (currentRanges.isEmpty() || validPoints.isEmpty()) return;

    // 计算位置
    QPointF pos;
    QString statusStr;

    if (calculatePosition(validPoints, validDistances, pos)) {
        m_mapWidget->updateTag(tagId, pos.x(), pos.y());
        statusStr = QString("Tag %1: (%2, %3) [Anchors:%4]").arg(tagId).arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1).arg(validPoints.size());
    } else {
        statusStr = QString("Tag %1: Positioning Failed (Valid Anchors<3)").arg(tagId);
    }

    m_tagInfoMap[tagId] = statusStr;
    updateTagStatusDisplay();
}

void MainWindow::updateTagStatusDisplay()
{
    QString allText;
    for (auto it = m_tagInfoMap.begin(); it != m_tagInfoMap.end(); ++it) {
        allText += it.value() + "\n";
    }
    m_txtTagInfo->setText(allText);
}

void MainWindow::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        QMessageBox::critical(this, "Connection Lost", "Serial device removed");
        toggleConnection();
    }
}

bool MainWindow::calculatePosition(const QVector<MapWidget::Point> &anchors, const QVector<double> &ranges, QPointF &result)
{
    int n = qMin(anchors.size(), ranges.size());
    if (n < 3) return false;

    struct ValidData { double x, y, r; };
    QVector<ValidData> data;
    for(int i=0; i<n; ++i) {
        if(ranges[i] > 0) data.append({anchors[i].x, anchors[i].y, ranges[i]});
    }

    if(data.size() < 3) return false;

    double x1 = data[0].x, y1 = data[0].y, r1 = data[0].r;
    double x2 = data[1].x, y2 = data[1].y, r2 = data[1].r;
    double x3 = data[2].x, y3 = data[2].y, r3 = data[2].r;

    double A = 2 * (x2 - x1);
    double B = 2 * (y2 - y1);
    double C = r1*r1 - r2*r2 - x1*x1 + x2*x2 - y1*y1 + y2*y2;

    double D = 2 * (x3 - x2);
    double E = 2 * (y3 - y2);
    double F = r2*r2 - r3*r3 - x2*x2 + x3*x3 - y2*y2 + y3*y3;

    double det = A * E - B * D;

    if (qAbs(det) < 1e-6) return false;

    double x = (C * E - B * F) / det;
    double y = (A * F - C * D) / det;

    result = QPointF(x, y);
    return true;
}

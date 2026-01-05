#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QMap>
#include <QPainter>
#include <QSettings>

// ==========================================
// MapWidget: 负责绘制基站和标签的画布
// ==========================================
class MapWidget : public QWidget {
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    struct Point {
        int x;
        int y;
    };

    struct Tag {
        Point pos;
        QColor color;
    };

    // 更新基站坐标
    void updateAnchors(const QVector<Point> &anchors);
    // 更新标签位置
    void updateTag(int id, int x, int y);
    void updateAnchorsMap(const QMap<int, Point> &anchorsMap);
    QMap<int, Point> getAnchorsMap() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<int, Point> m_anchors; // 基站 ID -> 坐标
    QMap<int, Tag> m_tags;      // 标签 ID -> 数据
    QPixmap m_anchorImage;

    // 绘图变换参数
    double m_scale;
    double m_offsetX;
    double m_offsetY;
    double m_margin;

    // 坐标转换辅助函数
    QPointF worldToScreen(double wx, double wy);
    void calculateTransform();
    void drawGrid(QPainter &painter);
};

// ==========================================
// MainWindow: 主界面与逻辑控制
// ==========================================
class QComboBox;
class QPushButton;
class QTableWidget;
class QLabel;
class QGroupBox;
class QDoubleSpinBox;
class QTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // UI 交互槽函数
    void refreshPorts();
    void toggleConnection();
    void applyAnchors();

    void onOpenExternalApp();

    // 串口槽函数
    void onSerialReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    // UI 初始化
    void initUI();
    void loadSettings();
    void saveSettings();
    void updateTagStatusDisplay(); // 刷新文本显示
    void logMessage(const QString &msg); // 新增：日志输出函数

    // 核心算法
    void processJsonData(const QByteArray &data);
    void processData(const QByteArray &data);
    bool calculatePosition(const QVector<MapWidget::Point> &anchors, const QVector<int> &ranges, QPoint &result);

    // 成员变量
    MapWidget *m_mapWidget;
    QSerialPort *m_serial;

    // UI 控件指针
    QComboBox *m_comboPorts;
    QPushButton *m_btnConnect;
    QTableWidget *m_tableAnchors;
    QDoubleSpinBox *m_spinThreshold;

    QLabel *m_lblConnection;     // 仅显示连接状态
    QTextEdit *m_txtLog;

    // 数据缓存
    QByteArray m_serialBuffer;
    QSettings *m_settings;

    QMap<int, QPoint> m_lastTagPoint;
};

#endif // MAINWINDOW_H

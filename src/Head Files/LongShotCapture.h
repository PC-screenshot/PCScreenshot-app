// LongShotCapture.h
#pragma once

#include <QObject>
#include <QRect>
#include <QVector>
#include <QPixmap>
#include <QPointer>
#include <QTimer>

class QWidget;
class QPainter;
class QKeyEvent;
class QWheelEvent;

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class LongShotCapture : public QObject
{
    Q_OBJECT
public:
    explicit LongShotCapture(QObject* parent = nullptr);

    void start(const QRect& selectionInOverlay, QWidget* overlayWidget);
    void stop();

    bool isActive() const { return active_; }

    void paintPreview(QPainter& painter, const QRect& widgetRect);

    bool handleKeyPress(QKeyEvent* event);
    bool handleWheel(QWheelEvent* event);

#ifdef Q_OS_WIN
    // 调试用：让 Overlay 能知道我们锁定的目标窗口
    HWND debugTargetWindow() const { return targetWindow_; }
#endif

private slots:
    void onTick();

private:
    bool active_ = false;
    QRect captureRectGlobal_;
    QVector<QPixmap> segments_;
    QPixmap previewPixmap_;
    QTimer timer_;
    QPointer<QWidget> overlay_;

#ifdef Q_OS_WIN
    long  oldExStyle_ = 0;
    bool  hasOldExStyle_ = false;
    HWND  targetWindow_ = nullptr;  // 仅用于抓图的实际窗口
#endif

    void updatePreview();
    bool isFrameDifferent(const QPixmap& a, const QPixmap& b);
    void finishAndExport();
};

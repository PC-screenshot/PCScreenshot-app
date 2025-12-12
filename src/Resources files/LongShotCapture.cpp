#include "LongShotCapture.h"

#include <QWidget>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QImage>
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QKeyEvent>
#include <QEventLoop>
#include <QDebug>
#include <QWheelEvent>
#include <QDebug>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

LongShotCapture::LongShotCapture(QObject* parent)
    : QObject(parent)
{
    // 200ms 一次，可以根据体验再调整
    timer_.setInterval(200);
    timer_.setSingleShot(false);
    connect(&timer_, &QTimer::timeout,
        this, &LongShotCapture::onTick);
}

void LongShotCapture::start(const QRect& selectionInOverlay,
    QWidget* overlayWidget)
{
    qDebug() << "[LongShot] start called, selectionInOverlay =" << selectionInOverlay
        << ", overlayWidget =" << overlayWidget;

    if (!overlayWidget || selectionInOverlay.isNull()) {
        qDebug() << "[LongShot] start aborted: overlayWidget or selection invalid";
        return;
    }

    stop();

    overlay_ = overlayWidget;

    QRect sel = selectionInOverlay.normalized();
    QPoint topLeftGlobal = overlayWidget->mapToGlobal(sel.topLeft());
    captureRectGlobal_ = QRect(topLeftGlobal, sel.size());

    qDebug() << "[LongShot] captureRectGlobal_ =" << captureRectGlobal_;

    segments_.clear();
    previewPixmap_ = QPixmap();

#ifdef Q_OS_WIN
    // 1) 让 Overlay 鼠标穿透
    if (overlay_) {
        HWND hwndOverlay = reinterpret_cast<HWND>(overlay_->winId());
        oldExStyle_ = GetWindowLong(hwndOverlay, GWL_EXSTYLE);
        hasOldExStyle_ = true;
        LONG newStyle = oldExStyle_ | WS_EX_TRANSPARENT;
        SetWindowLong(hwndOverlay, GWL_EXSTYLE, newStyle);
        qDebug() << "[LongShot] overlay WS_EX_TRANSPARENT enabled,"
            << "oldExStyle =" << QString::number(oldExStyle_, 16)
            << "newExStyle =" << QString::number(newStyle, 16);
    }

    // 2) 找选区中心下的真实窗口
    targetWindow_ = nullptr;

    POINT pt;
    pt.x = captureRectGlobal_.center().x();
    pt.y = captureRectGlobal_.center().y();

    HWND hwndOverlay = overlay_
        ? reinterpret_cast<HWND>(overlay_->winId())
        : nullptr;
    HWND hwnd = WindowFromPoint(pt);

    int guard = 0;
    while (hwnd && guard++ < 200) {
        if (hwnd == hwndOverlay) {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            continue;
        }
        if (!IsWindowVisible(hwnd)) {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            continue;
        }
        RECT wr{};
        if (!GetWindowRect(hwnd, &wr) || IsRectEmpty(&wr)) {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            continue;
        }
        if (!PtInRect(&wr, pt)) {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            continue;
        }
        targetWindow_ = hwnd;
        break;
    }

    if (targetWindow_) {
        wchar_t title[256] = { 0 };
        GetWindowTextW(targetWindow_, title, 255);
        qDebug() << "[LongShot] targetWindow_ =" << targetWindow_
            << "title =" << QString::fromWCharArray(title);
        SetForegroundWindow(targetWindow_);
    }
    else {
        qDebug() << "[LongShot] WARNING: failed to find targetWindow_";
    }
#endif

    active_ = true;
    qDebug() << "[LongShot] active_ set to true";

    onTick();
    if (overlay_) overlay_->update();

    timer_.start();
    qDebug() << "[LongShot] timer started, interval =" << timer_.interval();
}

void LongShotCapture::stop()
{
    if (!active_)
        return;

    timer_.stop();
    active_ = false;

#ifdef Q_OS_WIN
    // 恢复 overlay 的窗口样式
    if (hasOldExStyle_ && overlay_) {
        HWND hwndOverlay = reinterpret_cast<HWND>(overlay_->winId());
        SetWindowLong(hwndOverlay, GWL_EXSTYLE, oldExStyle_);
        qDebug() << "[LongShot] overlay WS_EX_TRANSPARENT cleared,"
            << "restoreExStyle =" << QString::number(oldExStyle_, 16);
        hasOldExStyle_ = false;
    }

    targetWindow_ = nullptr;
#endif
}

void LongShotCapture::onTick()
{
    if (!active_ || !overlay_)
        return;

    if (!captureRectGlobal_.isValid())
        return;

    // 1. 找到该区域所在屏幕
    QScreen* screen = QGuiApplication::screenAt(captureRectGlobal_.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    QPixmap frame;

#ifdef Q_OS_WIN
    if (targetWindow_) {
        // --- 优先从目标窗口抓图（不会有 Overlay，所以不会闪） ---
        RECT wr{};
        if (!GetWindowRect(targetWindow_, &wr)) {
            return;
        }

        QRect windowRect(wr.left,
            wr.top,
            wr.right - wr.left,
            wr.bottom - wr.top);

        // captureRectGlobal_ 与 windowRect 求交集，避免越界
        QRect inter = captureRectGlobal_.intersected(windowRect);
        if (inter.isEmpty())
            return;

        // 第一次时，用交集更新 captureRectGlobal_，保证后续尺寸一致
        if (segments_.isEmpty()) {
            captureRectGlobal_ = inter;
        }

        // 抓窗口局部区域：坐标相对于窗口左上角
        int x = inter.x() - windowRect.x();
        int y = inter.y() - windowRect.y();
        int w = inter.width();
        int h = inter.height();

        frame = screen->grabWindow(
            (WId)targetWindow_,
            x, y, w, h
        );
    }
    else
#endif
    {
        // --- 没拿到目标窗口时，退回到抓整个桌面（这时可能会包含 Overlay） ---
        const QRect& r = captureRectGlobal_;
        frame = screen->grabWindow(
            0,
            r.x(), r.y(),
            r.width(), r.height()
        );
    }

    if (frame.isNull())
        return;

    if (!segments_.isEmpty()) {
        const QPixmap& last = segments_.last();
        if (!isFrameDifferent(last, frame)) {
            qDebug() << "[LongShot] onTick: frame same as last, segments_ size ="
                << segments_.size();
            return;
        }
    }

    segments_.push_back(frame);
    qDebug() << "[LongShot] onTick: captured new frame, segments_ size ="
        << segments_.size();

    updatePreview();
    if (overlay_) overlay_->update();
}

bool LongShotCapture::isFrameDifferent(const QPixmap& a, const QPixmap& b)
{
    if (a.size() != b.size())
        return true;

    QImage ia = a.toImage().convertToFormat(QImage::Format_RGB32);
    QImage ib = b.toImage().convertToFormat(QImage::Format_RGB32);

    int w = ia.width();
    int h = ia.height();
    if (w <= 0 || h <= 0)
        return false;

    // 只关注“下方 1/3 区域”，更接近滚动时变化的区域
    int startY = h * 2 / 3;
    if (startY >= h) startY = 0;
    int endY = h - 1;

    int stepY = qMax(1, (endY - startY) / 30); // 最多 30 行
    int stepX = qMax(1, w / 50);               // 最多 50 列

    double diffSum = 0.0;
    int sampleCount = 0;

    for (int y = startY; y <= endY; y += stepY) {
        const QRgb* pa = reinterpret_cast<const QRgb*>(ia.constScanLine(y));
        const QRgb* pb = reinterpret_cast<const QRgb*>(ib.constScanLine(y));

        for (int x = 0; x < w; x += stepX) {
            QRgb ca = pa[x];
            QRgb cb = pb[x];

            int dr = qAbs(qRed(ca) - qRed(cb));
            int dg = qAbs(qGreen(ca) - qGreen(cb));
            int db = qAbs(qBlue(ca) - qBlue(cb));

            // 单像素三通道差值的平均
            double d = (dr + dg + db) / 3.0;
            diffSum += d;
            ++sampleCount;
        }
    }

    if (sampleCount == 0)
        return false;

    double avgDiff = diffSum / sampleCount;
    // 阈值：越大越“迟钝”，滚动一点才会认为有变化
    return avgDiff > 10.0;
}

void LongShotCapture::updatePreview()
{
    if (segments_.isEmpty()) {
        previewPixmap_ = QPixmap();
        return;
    }

    int w = segments_.first().width();
    int totalH = 0;
    for (const QPixmap& p : segments_) {
        totalH += p.height();
    }

    QImage img(w, totalH, QImage::Format_ARGB32);
    img.fill(Qt::white);

    QPainter painter(&img);
    int y = 0;
    for (const QPixmap& p : segments_) {
        painter.drawPixmap(0, y, p);
        y += p.height();
    }
    painter.end();

    previewPixmap_ = QPixmap::fromImage(img);
}

void LongShotCapture::paintPreview(QPainter& painter, const QRect& widgetRect)
{
    if (!active_ || previewPixmap_.isNull())
        return;

    painter.save();

    int previewWidth = widgetRect.width() / 4;   // 右侧 1/4 宽度
    int margin = 10;

    int x = widgetRect.x() + widgetRect.width() - previewWidth - margin;
    int y = widgetRect.y() + margin;
    int h = widgetRect.height() - 2 * margin;

    QRect panelRect(x, y, previewWidth, h);

    // 半透明背景
    painter.setBrush(QColor(0, 0, 0, 160));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(panelRect, 8, 8);

    QRect inner = panelRect.adjusted(8, 8, -8, -8);
    QPixmap scaled = previewPixmap_.scaled(
        inner.size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);

    QPoint topLeft = inner.topLeft();
    topLeft.setX(inner.x() + (inner.width() - scaled.width()) / 2);
    painter.drawPixmap(topLeft, scaled);

    painter.restore();
}

bool LongShotCapture::handleKeyPress(QKeyEvent* event)
{
    if (!active_)
        return false;

    if (event->key() == Qt::Key_Escape) {
        // ESC 结束并导出
        finishAndExport();
        return true;
    }
    return false;
}

bool LongShotCapture::handleWheel(QWheelEvent* event)
{
    Q_UNUSED(event);
    // QQ 模式：Overlay 鼠标穿透，真实窗口自己接收滚轮，我们不做任何事
    return false;
}

void LongShotCapture::finishAndExport()
{
    // 停掉定时器 & 恢复样式
    stop();

    if (segments_.isEmpty()) {
        if (overlay_) overlay_->close();
        return;
    }

    if (previewPixmap_.isNull()) {
        updatePreview();
    }
    if (previewPixmap_.isNull()) {
        if (overlay_) overlay_->close();
        return;
    }

    QPixmap result = previewPixmap_;

    // 1. 复制到剪贴板
    QGuiApplication::clipboard()->setPixmap(result);

    // 2. 另存为到本地
    const QString timestamp =
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    const QString default_file_name =
        QStringLiteral("qtscreenshot-long-%1.png").arg(timestamp);

    QString default_dir =
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (default_dir.isEmpty()) {
        default_dir = QDir::currentPath();
    }
    const QString default_path =
        QDir(default_dir).filePath(default_file_name);

    QString path = QFileDialog::getSaveFileName(
        overlay_,
        QObject::tr("保存长截图"),
        default_path,
        QObject::tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;Bitmap (*.bmp)")
    );

    if (!path.isEmpty()) {
        result.save(path);
    }

    if (overlay_) {
        overlay_->close();
    }
}

#include "ScreenshotOverlay.h"

#include <QPainter>
#include <QMouseEvent>

ScreenshotOverlay::ScreenshotOverlay(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::Tool);

    setWindowState(Qt::WindowFullScreen);
    setMouseTracking(true);
}

void ScreenshotOverlay::SetBackground(const QPixmap& pixmap) {
    background_ = pixmap;
    update();
}

void ScreenshotOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);

    painter.drawPixmap(0, 0, background_);

    painter.fillRect(rect(), QColor(0, 0, 0, 100));

    if (selecting_) {
        QRect sel = QRect(start_pos_, current_pos_).normalized();
        painter.setPen(QPen(Qt::blue, 2));
        painter.drawRect(sel);
    }
}

void ScreenshotOverlay::mousePressEvent(QMouseEvent* e) {
    selecting_ = true;
    start_pos_ = e->pos();
    current_pos_ = start_pos_;
    update();
}

void ScreenshotOverlay::mouseMoveEvent(QMouseEvent* e) {
    if (!selecting_) return;
    current_pos_ = e->pos();
    update();
}

void ScreenshotOverlay::mouseReleaseEvent(QMouseEvent* e) {
    selecting_ = false;
    current_pos_ = e->pos();

    QRect rect = QRect(start_pos_, current_pos_).normalized();
    emit RegionSelected(rect);

    close();
}

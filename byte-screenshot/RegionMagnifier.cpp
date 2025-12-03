#include "RegionMagnifier.h"

#include <QPainter>

RegionMagnifier::RegionMagnifier()
{
}

void RegionMagnifier::setSourcePixmap(const QPixmap* pixmap)
{
    source_ = pixmap;
}

void RegionMagnifier::setCursorPos(const QPoint& pos)
{
    cursor_pos_ = pos;
}

void RegionMagnifier::setEnabled(bool enabled)
{
    enabled_ = enabled;
}

void RegionMagnifier::setLensSize(int size)
{
    if (size < 20) size = 20;
    lens_size_ = size;
}

void RegionMagnifier::setZoomFactor(int factor)
{
    if (factor < 1) factor = 1;
    if (factor > 10) factor = 10;
    zoom_factor_ = factor;
}

void RegionMagnifier::paint(QPainter& painter, const QRect& widgetRect) const
{
    if (!enabled_ || !source_ || source_->isNull()) {
        return;
    }

    // 采样区域：以鼠标为中心，从源图像中裁一块
    const int half_src = lens_size_ / (2 * zoom_factor_);

    QRect srcRect(cursor_pos_.x() - half_src,
        cursor_pos_.y() - half_src,
        2 * half_src,
        2 * half_src);

    srcRect = srcRect.intersected(source_->rect());
    if (srcRect.isEmpty()) {
        return;
    }

    // 放大镜显示位置：默认在鼠标右下角偏一点
    int x = cursor_pos_.x() + 20;
    int y = cursor_pos_.y() + 20;

    // 避免超出窗口边界
    if (x + lens_size_ + 10 > widgetRect.right()) {
        x = cursor_pos_.x() - lens_size_ - 20;
    }
    if (x < widgetRect.left() + 10) {
        x = widgetRect.left() + 10;
    }

    if (y + lens_size_ + 10 > widgetRect.bottom()) {
        y = cursor_pos_.y() - lens_size_ - 20;
    }
    if (y < widgetRect.top() + 10) {
        y = widgetRect.top() + 10;
    }

    QRect targetRect(x, y, lens_size_, lens_size_);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 背景框
    QRect frameRect = targetRect.adjusted(-4, -4, 4, 4);
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.setBrush(QColor(30, 30, 30, 220));
    painter.drawRoundedRect(frameRect, 6, 6);

    // 放大后的图像
    painter.setClipRect(targetRect);
    painter.drawPixmap(targetRect, *source_, srcRect);
    painter.setClipping(false);

    // 十字准星
    painter.setPen(QPen(Qt::yellow, 1));
    painter.drawLine(targetRect.center().x(), targetRect.top(),
        targetRect.center().x(), targetRect.bottom());
    painter.drawLine(targetRect.left(), targetRect.center().y(),
        targetRect.right(), targetRect.center().y());

    painter.restore();
}

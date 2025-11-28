#include "ShapeDrawer.h"

#include <QtMath>
#include <QPen>
#include <QBrush>

namespace ShapeDrawer {

    static void PreparePainterForFill(QPainter& painter, const QColor& color, int penWidth, bool fill) {
        QPen pen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        if (fill) {
            painter.setBrush(QBrush(color));
        }
        else {
            painter.setBrush(Qt::NoBrush);
        }
    }

    // 绘制到 QPixmap
    void DrawRect(QPixmap& target, const QRect& rect, const QColor& color, int penWidth, bool fill) {
        QPainter painter(&target);
        painter.setRenderHint(QPainter::Antialiasing, true);
        PreparePainterForFill(painter, color, penWidth, fill);
        painter.drawRect(rect.normalized());
    }

    void DrawEllipse(QPixmap& target, const QRect& rect, const QColor& color, int penWidth, bool fill) {
        QPainter painter(&target);
        painter.setRenderHint(QPainter::Antialiasing, true);
        PreparePainterForFill(painter, color, penWidth, fill);
        painter.drawEllipse(rect.normalized());
    }

    void DrawArrow(QPixmap& target, const QPoint& p1, const QPoint& p2, const QColor& color, int penWidth) {
        QPainter painter(&target);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        const QPointF a = p1;
        const QPointF b = p2;
        painter.drawLine(a, b);

        // 箭头
        const double arrowLen = qMax(12.0, penWidth * 4.0);
        const double angle = std::atan2(b.y() - a.y(), b.x() - a.x());
        const double pi = 3.14159265358979323846;
        const double ang1 = angle + pi * 5.0 / 6.0; // 150 degrees
        const double ang2 = angle - pi * 5.0 / 6.0;

        QPointF head1 = b + QPointF(std::cos(ang1) * arrowLen, std::sin(ang1) * arrowLen);
        QPointF head2 = b + QPointF(std::cos(ang2) * arrowLen, std::sin(ang2) * arrowLen);

        QPolygonF poly;
        poly << b << head1 << head2;
        painter.setBrush(color);
        painter.drawPolygon(poly);
    }

    // 预览：虚线 / 半透明笔触，不改变目标
    static QPen PreviewPen(const QColor& color, int penWidth) {
        QPen pen(color, penWidth, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        QColor c = color;
        c.setAlpha(200);
        pen.setColor(c);
        return pen;
    }

    void DrawRectPreview(QPainter& painter, const QRect& rect, const QColor& color, int penWidth) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen = PreviewPen(color, penWidth);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.normalized());
        painter.restore();
    }

    void DrawEllipsePreview(QPainter& painter, const QRect& rect, const QColor& color, int penWidth) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen = PreviewPen(color, penWidth);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(rect.normalized());
        painter.restore();
    }

    void DrawArrowPreview(QPainter& painter, const QPoint& p1, const QPoint& p2, const QColor& color, int penWidth) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen = PreviewPen(color, penWidth);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        const QPointF a = p1;
        const QPointF b = p2;
        painter.drawLine(a, b);

        const double arrowLen = qMax(12.0, penWidth * 4.0);
        const double angle = std::atan2(b.y() - a.y(), b.x() - a.x());
        const double pi = 3.14159265358979323846;
        const double ang1 = angle + pi * 5.0 / 6.0;
        const double ang2 = angle - pi * 5.0 / 6.0;

        QPointF head1 = b + QPointF(std::cos(ang1) * arrowLen, std::sin(ang1) * arrowLen);
        QPointF head2 = b + QPointF(std::cos(ang2) * arrowLen, std::sin(ang2) * arrowLen);

        QPolygonF poly;
        poly << b << head1 << head2;
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 160));
        painter.drawPolygon(poly);

        painter.restore();
    }

} // namespace ShapeDrawer
#include "ShapeDrawer.h"

#include <QtMath>
#include <QPen>
#include <QBrush>
#include <QPainterPath>

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
/**
 * @brief 在给定的QPixmap上绘制矩形
 * @param target 目标QPixmap图像，用于绘制矩形
 * @param rect 要绘制的矩形的区域和大小
 * @param color 矩形的颜色
 * @param penWidth 矩形边框的宽度
 * @param fill 是否填充矩形内部
 */
void DrawRect(QPixmap& target, const QRect& rect, const QColor& color, int penWidth, bool fill) {
    // 创建绘制器，并将其关联到目标QPixmap
    QPainter painter(&target);
    // 启用抗锯齿渲染，使边缘更平滑
    painter.setRenderHint(QPainter::Antialiasing, true);
    // 设置绘制器的填充和画笔属性
    PreparePainterForFill(painter, color, penWidth, fill);
    // 绘制矩形，使用normalized()确保矩形具有正的宽度和高度
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



    // 添加Pen绘制功能
    void DrawPen(QPixmap& target, const QVector<QPoint>& path, const QColor& color, int penWidth) {
        if (path.isEmpty()) return;

        QPainter painter(&target);
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (path.size() == 1) {
            // 只有一个点，绘制一个圆点
            QPen pen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.drawPoint(path.first());
        }
        else {
            // 多个点，绘制路径
            QPen pen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);

            QPainterPath pathPainter;
            pathPainter.moveTo(path.first());
            for (int i = 1; i < path.size(); ++i) {
                pathPainter.lineTo(path.at(i));
            }
            painter.drawPath(pathPainter);
        }
    }

    // 橡皮擦功能
    void EraseObject(QPixmap& target, const QRect& area) {
        QPainter painter(&target);
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(area, Qt::transparent);
    }

    void EraseArea(QPixmap& target, const QRect& area) {
        QPainter painter(&target);
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(area, Qt::transparent);
    }

    // 添加橡皮擦功能：沿路径擦除
    void ErasePen(QPixmap& target, const QVector<QPoint>& path, int penWidth) {
        if (path.isEmpty()) return;

        QPainter painter(&target);
        painter.setCompositionMode(QPainter::CompositionMode_Clear);

        if (path.size() == 1) {
            // 只有一个点，擦除一个圆点区域
            QPen pen(Qt::transparent, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            painter.drawPoint(path.first());
        }
        else {
            // 多个点，沿路径擦除
            QPen pen(Qt::transparent, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);

            QPainterPath pathPainter;
            pathPainter.moveTo(path.first());
            for (int i = 1; i < path.size(); ++i) {
                pathPainter.lineTo(path.at(i));
            }
            painter.drawPath(pathPainter);
        }
    }
    // 预览功能：修改 painter 以便在使用前进行 translate/scale
    void DrawRectPreview(QPainter& painter, const QRect& rect, const QColor& color, int penWidth) {
        PreparePainterForFill(painter, color, penWidth, false);
        painter.drawRect(rect.normalized());
    }

    void DrawEllipsePreview(QPainter& painter, const QRect& rect, const QColor& color, int penWidth) {
        PreparePainterForFill(painter, color, penWidth, false);
        painter.drawEllipse(rect.normalized());
    }

    void DrawArrowPreview(QPainter& painter, const QPoint& p1, const QPoint& p2, const QColor& color, int penWidth) {
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

    void DrawPolygonPreview(QPainter& painter, const QPolygon& polygon, const QColor& color, int penWidth) {
        PreparePainterForFill(painter, color, penWidth, false);
        painter.drawPolygon(polygon);
    }

    // 添加Pen预览功能
    void DrawPenPreview(QPainter& painter, const QVector<QPoint>& path, const QColor& color, int penWidth) {
        if (path.isEmpty()) return;

        painter.setRenderHint(QPainter::Antialiasing, true);

        if (path.size() == 1) {
            // 只有一个点，绘制一个圆点
            QPen pen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.drawPoint(path.first());
        }
        else {
            // 多个点，绘制路径
            QPen pen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);

            QPainterPath pathPainter;
            pathPainter.moveTo(path.first());
            for (int i = 1; i < path.size(); ++i) {
                pathPainter.lineTo(path.at(i));
            }
            painter.drawPath(pathPainter);
        }
    }
}
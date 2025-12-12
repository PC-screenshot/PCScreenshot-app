#pragma once

#include <QPixmap>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QColor>

namespace ShapeDrawer {

	// 绘制形状到目标 QPixmap，直接修改 target
	void DrawRect(QPixmap& target, const QRect& rect, const QColor& color, int penWidth = 2, bool fill = false);
	void DrawEllipse(QPixmap& target, const QRect& rect, const QColor& color, int penWidth = 2, bool fill = false);
	void DrawArrow(QPixmap& target, const QPoint& p1, const QPoint& p2, const QColor& color, int penWidth = 2);
	void DrawPen(QPixmap& target, const QVector<QPoint>& path, const QColor& color, int penWidth = 2); // 添加Pen功能
	// 橡皮擦功能
	void EraseObject(QPixmap& target, const QRect& area); // 对象橡皮擦
	void EraseArea(QPixmap& target, const QRect& area);   // 普通橡皮擦
	void ErasePen(QPixmap& target, const QVector<QPoint>& path, int penWidth = 2); // 橡皮擦功能：沿路径擦除
	// 预览功能，修改 painter，使用时需提前设置 translate/scale
	void DrawRectPreview(QPainter& painter, const QRect& rect, const QColor& color, int penWidth = 1);
	void DrawEllipsePreview(QPainter& painter, const QRect& rect, const QColor& color, int penWidth = 1);
	void DrawArrowPreview(QPainter& painter, const QPoint& p1, const QPoint& p2, const QColor& color, int penWidth = 1);
	void DrawPolygonPreview(QPainter& painter, const QPolygon& polygon, const QColor& color, int penWidth = 1);
	void DrawPenPreview(QPainter& painter, const QVector<QPoint>& path, const QColor& color, int penWidth = 1); // 添加Pen预览功能
} // namespace ShapeDrawer#pragma once#pragma once

#pragma once

#include <QPixmap>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QColor>

namespace ShapeDrawer {

	// 将形状直接绘制到目标 QPixmap（会修改 target）
	void DrawRect(QPixmap& target, const QRect& rect, const QColor& color, int penWidth = 2, bool fill = false);
	void DrawEllipse(QPixmap& target, const QRect& rect, const QColor& color, int penWidth = 2, bool fill = false);
	void DrawArrow(QPixmap& target, const QPoint& p1, const QPoint& p2, const QColor& color, int penWidth = 2);

	// 预览用（不修改目标，使用传入的 QPainter，在调用方已经设置好坐标系/translate）
	void DrawRectPreview(QPainter& painter, const QRect& rect, const QColor& color, int penWidth = 1);
	void DrawEllipsePreview(QPainter& painter, const QRect& rect, const QColor& color, int penWidth = 1);
	void DrawArrowPreview(QPainter& painter, const QPoint& p1, const QPoint& p2, const QColor& color, int penWidth = 1);

} // namespace ShapeDrawer#pragma once

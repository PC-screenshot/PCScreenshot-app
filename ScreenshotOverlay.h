#pragma once

#include <QWidget>
#include <QPixmap>
#include <QRect>

class ScreenshotOverlay : public QWidget {
	Q_OBJECT
public:
	explicit ScreenshotOverlay(QWidget* parent = nullptr);

	void SetBackground(const QPixmap& pixmap);

signals:
	void RegionSelected(const QRect& rect);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	QPixmap background_;
	QPoint start_pos_;
	QPoint current_pos_;
	bool selecting_ = false;
};

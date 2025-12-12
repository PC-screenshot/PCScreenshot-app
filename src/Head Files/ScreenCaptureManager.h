#pragma once

#include <QObject>
#include <QPixmap>
#include <QRect>

class ScreenCaptureManager : public QObject {
	Q_OBJECT
public:
	explicit ScreenCaptureManager(QObject* parent = nullptr);

	QPixmap CaptureFullScreen();
	QPixmap CaptureRect(const QRect& rect);
};
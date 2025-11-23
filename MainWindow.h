#pragma once

#include <QWidget>
#include "ScreenCaptureManager.h"
#include "ScreenshotOverlay.h"

class MainWindow : public QWidget {
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);

private slots:
	void OnStartCapture();
	void OnRegionSelected(const QRect& rect);

private:
	ScreenCaptureManager capture_manager_;
};

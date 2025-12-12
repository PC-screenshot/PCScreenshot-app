// MainWindow.h
#pragma once

#include <QWidget>
#include <QSystemTrayIcon>
#include <QMenu>

#include "ScreenshotOverlay.h"
#include "ScreenCaptureManager.h"

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void OnStartCapture();      // 截图入口

private:
    void createTrayIcon();      // 创建托盘图标和菜单

    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;

    ScreenCaptureManager   capture_manager_;
};

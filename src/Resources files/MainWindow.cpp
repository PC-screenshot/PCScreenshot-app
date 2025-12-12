// MainWindow.cpp
#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QIcon>

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    // 这个窗口我们不显示，所以不需要布局、按钮
    hide();
    setWindowTitle("Screenshot Tool");
    setWindowIcon(QIcon(":/icons/icons8-cut-64.png"));  

    createTrayIcon();
}

void MainWindow::createTrayIcon()
{
    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setIcon(QIcon(":/icons/icons8-cut-64.png")); // 托盘图标

    // 托盘右键菜单
    trayMenu_ = new QMenu();

    QAction* actCapture = trayMenu_->addAction("Capture Screen");
    QAction* actQuit = trayMenu_->addAction("Quit");

    trayIcon_->setContextMenu(trayMenu_);
    trayIcon_->show();

    // 右键菜单 -> 截图
    connect(actCapture, &QAction::triggered,
        this, &MainWindow::OnStartCapture);

    // 右键菜单 -> 退出
    connect(actQuit, &QAction::triggered,
        qApp, &QCoreApplication::quit);

    // 左键单击托盘图标也直接截图
    connect(trayIcon_, &QSystemTrayIcon::activated,
        this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {   // 左键单击
                OnStartCapture();
            }
        });
}

void MainWindow::OnStartCapture()
{
    // 1. 先截一张整屏图
    QPixmap full = capture_manager_.CaptureFullScreen();

    // 2. 创建截图/编辑界面
    auto* overlay = new ScreenshotOverlay(nullptr);
    overlay->SetBackground(full);

    // 窗口关闭时自动 delete
    overlay->setAttribute(Qt::WA_DeleteOnClose);

    overlay->show();
}

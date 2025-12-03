#include "MainWindow.h"

#include <QPushButton>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);

    QPushButton* btn = new QPushButton("Start Capture", this);
    layout->addWidget(btn);

    connect(btn, &QPushButton::clicked,
        this, &MainWindow::OnStartCapture);

    resize(300, 200);
    setWindowTitle("Screenshot Tool");
}

void MainWindow::OnStartCapture() {
    // 1. 先截一张整屏图
    QPixmap full = capture_manager_.CaptureFullScreen();

    // 2. 创建截图/编辑界面
    auto* overlay = new ScreenshotOverlay(nullptr);
    overlay->SetBackground(full);

    // 窗口关闭时自动 delete
    overlay->setAttribute(Qt::WA_DeleteOnClose);

    overlay->show();
}

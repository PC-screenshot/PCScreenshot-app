#include "MainWindow.h"

#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QPushButton* btn = new QPushButton("Start Capture", this);
    layout->addWidget(btn);

    connect(btn, &QPushButton::clicked,
        this, &MainWindow::OnStartCapture);

    resize(300, 200);
    setWindowTitle("Screenshot Tool (Qt6 + QWidget)");
}

void MainWindow::OnStartCapture() {
    QPixmap full = capture_manager_.CaptureFullScreen();

    ScreenshotOverlay* overlay = new ScreenshotOverlay();
    overlay->SetBackground(full);

    connect(overlay, &ScreenshotOverlay::RegionSelected,
        this, &MainWindow::OnRegionSelected);

    overlay->show();
}

void MainWindow::OnRegionSelected(const QRect& rect) {
    QPixmap shot = capture_manager_.CaptureRect(rect);

    // New window to show the screenshot
    QWidget* preview = new QWidget();
    preview->setAttribute(Qt::WA_TranslucentBackground);

    // Remove ALL borders, title bar, frame
    preview->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);

    // Create a label to display the image
    QLabel* label = new QLabel(preview);
    label->setPixmap(shot);
    label->resize(shot.size());

    // Resize preview window to fit the image exactly
    preview->resize(shot.size());

    preview->show();
}

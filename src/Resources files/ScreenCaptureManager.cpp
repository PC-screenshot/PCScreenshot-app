#include "ScreenCaptureManager.h"

#include <QGuiApplication>
#include <QScreen>

ScreenCaptureManager::ScreenCaptureManager(QObject* parent)
    : QObject(parent) {
}

QPixmap ScreenCaptureManager::CaptureFullScreen() {
    QScreen* screen = QGuiApplication::primaryScreen();
    return screen ? screen->grabWindow(0) : QPixmap();
}

QPixmap ScreenCaptureManager::CaptureRect(const QRect& rect) {
    QScreen* screen = QGuiApplication::primaryScreen();
    return screen ? screen->grabWindow(0, rect.x(), rect.y(),
        rect.width(), rect.height())
        : QPixmap();
}

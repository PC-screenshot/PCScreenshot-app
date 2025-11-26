#include "PinnedWindow.h"

#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QWheelEvent>

PinnedWindow::PinnedWindow(const QPixmap& pixmap, QWidget* parent)
    : QWidget(parent), pixmap_(pixmap) {
  setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle("Pinned Screenshot");
  setFocusPolicy(Qt::StrongFocus);

  if (!pixmap_.isNull()) {
    resize(pixmap_.size());
  }
}

void PinnedWindow::resizeEvent(QResizeEvent* /*event*/) {
}

void PinnedWindow::paintEvent(QPaintEvent* /*event*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  if (!pixmap_.isNull()) {
    if (isMaximized()) {
      QSize s = (pixmap_.size() * zoom_scale_).boundedTo(size());
      QPixmap scaled = pixmap_.scaled(s, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      QPoint topLeft((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
      p.drawPixmap(topLeft, scaled);
    } else {
      QPoint topLeft((width() - pixmap_.width()) / 2, (height() - pixmap_.height()) / 2);
      p.drawPixmap(topLeft, pixmap_);
    }
  }
}

void PinnedWindow::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    dragging_ = true;
    drag_offset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
  }
}

void PinnedWindow::mouseMoveEvent(QMouseEvent* event) {
  if (dragging_ && (event->buttons() & Qt::LeftButton)) {
    move(event->globalPosition().toPoint() - drag_offset_);
  }
}

void PinnedWindow::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    dragging_ = false;
  }
}

void PinnedWindow::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    close();
  }
}

void PinnedWindow::Pinned(const QPixmap& pixmap, QWidget* parent) {
  if (pixmap.isNull()) {
    return;
  }
  auto* w = new PinnedWindow(pixmap, parent);
  w->show();
  w->raise();
}

void PinnedWindow::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape) {
    close();
    return;
  }
  QWidget::keyPressEvent(event);
}

void PinnedWindow::wheelEvent(QWheelEvent* event) {
  if (!isMaximized() || pixmap_.isNull()) return;
  const int delta = event->angleDelta().y();
  if (delta == 0) return;
  double factor = (delta > 0) ? 1.1 : 0.9;
  zoom_scale_ *= factor;
  if (zoom_scale_ < 0.2) zoom_scale_ = 0.2;
  if (zoom_scale_ > 8.0) zoom_scale_ = 8.0;
  update();
}

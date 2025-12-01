#pragma once

#include <QWidget>
#include <QPixmap>
#include <QWheelEvent>

class PinnedWindow : public QWidget {
  Q_OBJECT

 public:
  explicit PinnedWindow(const QPixmap& pixmap, QWidget* parent = nullptr);

  static void Pinned(const QPixmap& pixmap, QWidget* parent = nullptr);

  protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  QPixmap pixmap_;
  QPoint drag_offset_;
  bool dragging_ = false;
  double zoom_scale_ = 1.0;
};


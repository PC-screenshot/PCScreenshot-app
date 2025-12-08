#pragma once

#include <QWidget>
#include <QPixmap>
#include <QWheelEvent>
#include <QToolButton>
#include <QPushButton>
#include <QEnterEvent>

class PinnedWindow : public QWidget {
  Q_OBJECT

 public:
  explicit PinnedWindow(const QPixmap& pixmap, QWidget* parent = nullptr);

  static PinnedWindow* CreatePinnedWindow(const QPixmap& pixmap, QWidget* parent = nullptr);

  // 设置是否启用 OCR 按钮
  void setOcrEnabled(bool enabled);

 signals:
  
  // 窗口关闭信号
  void windowClosed();

 protected:
  void closeEvent(QCloseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void moveEvent(QMoveEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void OnCopy();
  void OnSave();
  void OnOcr();
  void OnTogglePin();

 private:
  void SetupUi();
  void UpdatePinButtonState();
  void UpdateToolbarPosition();
  void CheckHideUi();

  QPixmap pixmap_;
  QPoint drag_offset_;
  bool dragging_ = false;

  // UI Elements
  QPushButton* close_btn_ = nullptr;
  QWidget* tool_bar_ = nullptr;
  QToolButton* btn_copy_ = nullptr;
  QToolButton* btn_save_ = nullptr;
  QToolButton* btn_ocr_ = nullptr;
  QToolButton* btn_pin_ = nullptr;

  bool is_pinned_ = true;
};


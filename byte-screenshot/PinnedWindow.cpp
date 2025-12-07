#include "PinnedWindow.h"
#include "OCR.h"
#include "OcrResultDialog.h"

#include <QMouseEvent>
#include <QMoveEvent>
#include <QCloseEvent>
#include <QPainter>
#include <QStyle>
#include <QWheelEvent>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QApplication>
#include <QTimer>
#include <QToolTip>
#include <QPointer>
#include <QDialog>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
    // 复制自 EditorToolbar.cpp
    const char* kSelectableButtonStyle = R"(
  QToolButton {
    color: #444444;
    background: transparent;
    border-radius: 4px;
    padding: 4px 6px;
  }
  QToolButton:hover {
    background: rgba(0, 0, 0, 0.08);
  }
  QToolButton:checked {
    background: rgba(0, 0, 0, 0.18);
    border: 1px solid #666666;
  }
)";

    const char* kActionButtonStyle = R"(
  QToolButton {
    color: #555555;
    background: transparent;
    border-radius: 4px;
    padding: 4px 6px;
  }
  QToolButton:hover {
    background: rgba(0, 0, 0, 0.06);
  }
)";
}

PinnedWindow::PinnedWindow(const QPixmap& pixmap, QWidget* parent)
    : QWidget(parent), pixmap_(pixmap) {
  // 设置无边框、置顶、工具窗口属性
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle("Pinned Screenshot");
  setMouseTracking(true); // 启用鼠标追踪，以便处理 enter/leave 事件

  if (!pixmap_.isNull()) {
    resize(pixmap_.size());
  }

  SetupUi();
}

void PinnedWindow::SetupUi() {
  // 1. 关闭按钮 (右上角)
  close_btn_ = new QPushButton(this);
  close_btn_->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
  close_btn_->setFixedSize(24, 24);
  close_btn_->setFlat(true);
  close_btn_->setStyleSheet(
      "QPushButton { background-color: rgba(200, 0, 0, 0.5); border-radius: 12px; border: none; }"
      "QPushButton:hover { background-color: rgba(255, 0, 0, 0.8); }");
  close_btn_->setCursor(Qt::ArrowCursor);
  connect(close_btn_, &QPushButton::clicked, this, &QWidget::close);

  // 2. 底部工具栏容器
  tool_bar_ = new QWidget(this);
  // 设置为独立的工具窗口，无边框，置顶
  tool_bar_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  // 安装事件过滤器以检测鼠标离开
  tool_bar_->installEventFilter(this);
  
  // 模仿 EditorToolbar 的背景样式
  tool_bar_->setAttribute(Qt::WA_StyledBackground);
  tool_bar_->setStyleSheet(
      "QWidget { background-color: rgba(245, 245, 245, 240); border: 1px solid #C8C8C8; border-radius: 6px; }");
  
  auto* layout = new QHBoxLayout(tool_bar_);
  layout->setContentsMargins(6, 4, 6, 4);
  layout->setSpacing(2); // EditorToolbar 是 2

  // Copy 按钮
  btn_copy_ = new QToolButton(tool_bar_);
  btn_copy_->setText("Copy");
  btn_copy_->setToolTip("Copy to Clipboard");
  // 移除图标，仅保留文字以匹配 EditorToolbar
  btn_copy_->setFixedSize(60, 28);
  btn_copy_->setAutoRaise(true);
  btn_copy_->setStyleSheet(kActionButtonStyle);
  connect(btn_copy_, &QToolButton::clicked, this, &PinnedWindow::OnCopy);
  layout->addWidget(btn_copy_);

  // Save 按钮
  btn_save_ = new QToolButton(tool_bar_);
  btn_save_->setText("Save");
  btn_save_->setToolTip("Save to File");
  btn_save_->setFixedSize(60, 28);
  btn_save_->setAutoRaise(true);
  btn_save_->setStyleSheet(kActionButtonStyle);
  connect(btn_save_, &QToolButton::clicked, this, &PinnedWindow::OnSave);
  layout->addWidget(btn_save_);

  // OCR 按钮
  btn_ocr_ = new QToolButton(tool_bar_);
  btn_ocr_->setText("OCR");
  btn_ocr_->setToolTip("Extract Text");
  btn_ocr_->setFixedSize(60, 28);
  btn_ocr_->setAutoRaise(true);
  btn_ocr_->setStyleSheet(kActionButtonStyle);
  connect(btn_ocr_, &QToolButton::clicked, this, &PinnedWindow::OnOcr);
  layout->addWidget(btn_ocr_);
  // 默认隐藏，由外部控制开启，或者默认开启
  btn_ocr_->hide(); 

  // Pin 按钮 (切换置顶)
  btn_pin_ = new QToolButton(tool_bar_);
  btn_pin_->setText("Pin"); // 初始文字
  btn_pin_->setToolTip("Toggle Always on Top");
  btn_pin_->setFixedSize(60, 28);
  btn_pin_->setCheckable(true);
  btn_pin_->setChecked(true); // 默认置顶
  btn_pin_->setAutoRaise(true);
  btn_pin_->setStyleSheet(kSelectableButtonStyle);
  connect(btn_pin_, &QToolButton::clicked, this, &PinnedWindow::OnTogglePin);
  layout->addWidget(btn_pin_);

  // 调整工具栏大小以适应内容
  tool_bar_->adjustSize();

  // 初始状态：更新 Pin 按钮样式
  UpdatePinButtonState();

  // 初始隐藏 UI，鼠标移入时显示
  close_btn_->hide();
  tool_bar_->hide();
}

void PinnedWindow::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);

  // 保持关闭按钮在右上角
  if (close_btn_) {
    close_btn_->move(width() - close_btn_->width() - 5, 5);
  }

  UpdateToolbarPosition();
}

void PinnedWindow::closeEvent(QCloseEvent* event) {
    emit windowClosed();
    QWidget::closeEvent(event);
}

void PinnedWindow::paintEvent(QPaintEvent* /*event*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  
  // 绘制背景图，铺满整个窗口
  if (!pixmap_.isNull()) {
    p.drawPixmap(rect(), pixmap_);
  }

  // 绘制细边框
  p.setPen(QPen(QColor(0, 0, 0, 50), 1));
  p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void PinnedWindow::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    // 如果点击在工具栏或关闭按钮上，不处理拖拽
    if (close_btn_ && close_btn_->isVisible() && close_btn_->geometry().contains(event->pos())) {
        return;
    }
    if (tool_bar_ && tool_bar_->isVisible() && tool_bar_->geometry().contains(event->pos())) {
        return;
    }

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

PinnedWindow* PinnedWindow::CreatePinnedWindow(const QPixmap& pixmap, QWidget* parent) {
  if (pixmap.isNull()) {
    return nullptr;
  }
  auto* w = new PinnedWindow(pixmap, parent);
  w->show();
  w->raise();
  return w;
}

void PinnedWindow::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape) {
    close();
    return;
  }
  // 快捷键支持
  if (event->matches(QKeySequence::Copy)) {
      OnCopy();
      return;
  }
  if (event->matches(QKeySequence::Save)) {
      OnSave();
      return;
  }
  QWidget::keyPressEvent(event);
}

void PinnedWindow::wheelEvent(QWheelEvent* event) {
  const int delta = event->angleDelta().y();
  if (delta == 0) return;

  // 支持 Ctrl + 滚轮调节透明度
  if (event->modifiers() == Qt::ControlModifier) {
      qreal opacity = windowOpacity();
      if (delta > 0) {
          opacity = std::min(opacity + 0.1, 1.0);
      } else {
          opacity = std::max(opacity - 0.1, 0.2); // 最低 0.2，防止完全消失
      }
      setWindowOpacity(opacity);
      
      // 显示当前透明度提示
      QString tip = QString("Opacity: %1%").arg(int(opacity * 100));
      QToolTip::showText(QCursor::pos(), tip, this);
      return;
  }

  // 滚轮调整窗口大小
  double factor = (delta > 0) ? 1.1 : 0.9;
  
  // 计算新尺寸
  QSize new_size = size() * factor;
  
  // 最小尺寸限制
  if (new_size.width() < 50 || new_size.height() < 50) {
      return;
  }

  // 最大尺寸限制 (防止过大)
  if (new_size.width() > 5000 || new_size.height() > 5000) {
      return;
  }
  
  resize(new_size);
}

void PinnedWindow::enterEvent(QEnterEvent* event) {
    if (close_btn_) close_btn_->show();
    if (tool_bar_) {
        UpdateToolbarPosition();
        tool_bar_->show();
    }
    QWidget::enterEvent(event);
}

void PinnedWindow::leaveEvent(QEvent* event) {
    // 当鼠标离开窗口区域时，延迟检查是否需要隐藏 UI
    // 这样可以允许用户将鼠标移动到外部的工具栏上
    QTimer::singleShot(100, this, &PinnedWindow::CheckHideUi);
    QWidget::leaveEvent(event);
}

void PinnedWindow::moveEvent(QMoveEvent* event) {
  QWidget::moveEvent(event);
  UpdateToolbarPosition();
}

void PinnedWindow::UpdateToolbarPosition() {
  if (!tool_bar_) return;
  
  // 计算工具栏位置：主窗口右下角下方
  // 右对齐
  int x = this->x() + this->width() - tool_bar_->width();
  // 下方 5px，保持一定间距
  int y = this->y() + this->height() + 5;
  
  tool_bar_->move(x, y);
}

void PinnedWindow::CheckHideUi() {
  // 如果工具栏本身就没显示，或者已经被销毁，则无需处理
  if (!tool_bar_ || !tool_bar_->isVisible()) {
      // 确保 close_btn 也隐藏（如果鼠标也不在主窗口）
      if (close_btn_ && !frameGeometry().contains(QCursor::pos())) {
          close_btn_->hide();
      }
      return;
  }

  QPoint pos = QCursor::pos();
  bool in_main = frameGeometry().contains(pos);
  bool in_toolbar = tool_bar_->frameGeometry().contains(pos);
  
  if (!in_main && !in_toolbar) {
    if (close_btn_) close_btn_->hide();
    tool_bar_->hide();
  }
}

bool PinnedWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched == tool_bar_) {
    if (event->type() == QEvent::Leave) {
      // 鼠标离开工具栏，延迟检查是否隐藏
      QTimer::singleShot(100, this, &PinnedWindow::CheckHideUi);
    }
  }
  return QWidget::eventFilter(watched, event);
}

void PinnedWindow::OnCopy() {
    QClipboard* cb = QGuiApplication::clipboard();
    cb->setPixmap(pixmap_);
    
    // 简单的反馈：改变一下按钮文字或背景一瞬间
    btn_copy_->setText("Copied!");
    QTimer::singleShot(1000, this, [this]() {
        if (btn_copy_) btn_copy_->setText("Copy");
    });
}

void PinnedWindow::OnSave() {
    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Screenshot"),
        QString(),
        tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;Bitmap (*.bmp)"));
    
    if (!path.isEmpty()) {
        pixmap_.save(path);
    }
}

void PinnedWindow::OnTogglePin() {
    is_pinned_ = !is_pinned_;
    UpdatePinButtonState();
}

void PinnedWindow::OnOcr() {
    auto* engine = &OcrEngine::instance();
    
    // 创建对话框并显示（非模态，允许用户继续操作其他窗口）
    // 初始状态下文本可能为空，等待 OCR 结果
    auto* dlg = new OcrResultDialog(pixmap_, "Recognizing...", nullptr);
    dlg->show();
    dlg->raise();

    // 使用 QPointer 确保安全访问
    QPointer<OcrResultDialog> safeDlg(dlg);

    // 延迟执行 OCR，确保对话框先显示出来
    QTimer::singleShot(100, [engine, safeDlg, this]() {
        // 如果对话框在 OCR 开始前被关闭，则不再继续
        if (!safeDlg) return;

        QString result;
        try {
            // 同步调用 OCR (会阻塞 UI 线程，但由于已显示 Loading 对话框，用户体验尚可)
            result = engine->detectText(pixmap_.toImage());
        } catch (const std::exception& e) {
            result = QString("Error: %1").arg(e.what());
        } catch (...) {
            result = "Unknown Error during OCR dispatch.";
        }

        if (safeDlg) {
            safeDlg->SetResultText(result);
        }
    });
}

void PinnedWindow::setOcrEnabled(bool enabled) {
    if (btn_ocr_) {
        btn_ocr_->setVisible(enabled);
        if (tool_bar_) {
            tool_bar_->adjustSize();
        }
    }
}

void PinnedWindow::UpdatePinButtonState() {
    // 动态切换 WindowStaysOnTopHint
    bool visible = isVisible();
    
    if (is_pinned_) {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        if (btn_pin_) {
            btn_pin_->setChecked(true);
            // 保持样式表为 kSelectableButtonStyle，因为 checked 状态会自动处理高亮
        }
    } else {
        setWindowFlag(Qt::WindowStaysOnTopHint, false);
        if (btn_pin_) {
            btn_pin_->setChecked(false);
        }
    }
    
    if (visible) {
        show();
    }
}

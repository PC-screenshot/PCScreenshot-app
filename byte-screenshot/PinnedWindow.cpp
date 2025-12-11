#include "PinnedWindow.h"
#include "OCR.h"
#include "OcrResultDialog.h"

#include <QMouseEvent>
#include <QMoveEvent>
#include <QCloseEvent>
#include <QPainter>
#include <QStyle>
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
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle("Pinned Screenshot");
  setMouseTracking(true);

  if (!pixmap_.isNull()) {
    resize(pixmap_.size());
  }

  SetupUi();
}


// init
void PinnedWindow::SetupUi() {
  close_btn_ = new QPushButton(this);
  close_btn_->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
  close_btn_->setFixedSize(24, 24);
  close_btn_->setFlat(true);
  close_btn_->setStyleSheet(
      "QPushButton { background-color: rgba(200, 0, 0, 0.5); border-radius: 12px; border: none; }"
      "QPushButton:hover { background-color: rgba(255, 0, 0, 0.8); }");
  close_btn_->setCursor(Qt::ArrowCursor);
  connect(close_btn_, &QPushButton::clicked, this, &QWidget::close);

  tool_bar_ = new QWidget(this);
  tool_bar_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  tool_bar_->installEventFilter(this);
  
  tool_bar_->setAttribute(Qt::WA_StyledBackground);
  tool_bar_->setStyleSheet(
      "QWidget { background-color: rgba(245, 245, 245, 240); border: 1px solid #C8C8C8; border-radius: 6px; }");
  
  auto* layout = new QHBoxLayout(tool_bar_);
  layout->setContentsMargins(6, 4, 6, 4);
  layout->setSpacing(2);

  // Copy
  btn_copy_ = new QToolButton(tool_bar_);
  btn_copy_->setText("Copy");
  btn_copy_->setToolTip("Copy to Clipboard");
  btn_copy_->setFixedSize(60, 28);
  btn_copy_->setAutoRaise(true);
  btn_copy_->setStyleSheet(kActionButtonStyle);
  connect(btn_copy_, &QToolButton::clicked, this, &PinnedWindow::OnCopy);
  layout->addWidget(btn_copy_);

  // Save
  btn_save_ = new QToolButton(tool_bar_);
  btn_save_->setText("Save");
  btn_save_->setToolTip("Save to File");
  btn_save_->setFixedSize(60, 28);
  btn_save_->setAutoRaise(true);
  btn_save_->setStyleSheet(kActionButtonStyle);
  connect(btn_save_, &QToolButton::clicked, this, &PinnedWindow::OnSave);
  layout->addWidget(btn_save_);

  // OCR
  btn_ocr_ = new QToolButton(tool_bar_);
  btn_ocr_->setText("OCR");
  btn_ocr_->setToolTip("Extract Text");
  btn_ocr_->setFixedSize(60, 28);
  btn_ocr_->setAutoRaise(true);
  btn_ocr_->setStyleSheet(kActionButtonStyle);
  connect(btn_ocr_, &QToolButton::clicked, this, &PinnedWindow::OnOcr);
  layout->addWidget(btn_ocr_);
  btn_ocr_->hide(); 

  // Pin
  btn_pin_ = new QToolButton(tool_bar_);
  btn_pin_->setText("Pin");
  btn_pin_->setToolTip("Toggle Always on Top");
  btn_pin_->setFixedSize(60, 28);
  btn_pin_->setCheckable(true);
  btn_pin_->setChecked(true);
  btn_pin_->setAutoRaise(true);
  btn_pin_->setStyleSheet(kSelectableButtonStyle);
  connect(btn_pin_, &QToolButton::clicked, this, &PinnedWindow::OnTogglePin);
  layout->addWidget(btn_pin_);

  tool_bar_->adjustSize();

  UpdatePinButtonState();

  close_btn_->hide();
  tool_bar_->hide();
}

void PinnedWindow::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);

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
  
  if (!pixmap_.isNull()) {
    p.drawPixmap(rect(), pixmap_);
  }

  p.setPen(QPen(QColor(0, 0, 0, 50), 1));
  p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// move
void PinnedWindow::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
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


// create
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

// Opacity & Size
void PinnedWindow::wheelEvent(QWheelEvent* event) {
  const int delta = event->angleDelta().y();
  if (delta == 0) return;

  if (event->modifiers() == Qt::ControlModifier) {
      qreal opacity = windowOpacity();
      if (delta > 0) {
          opacity = std::min(opacity + 0.1, 1.0);
      } else {
          opacity = std::max(opacity - 0.1, 0.2);
      }
      setWindowOpacity(opacity);
      
      QString tip = QString("Opacity: %1%").arg(int(opacity * 100));
      QToolTip::showText(QCursor::pos(), tip, this);
      return;
  }

  double factor = (delta > 0) ? 1.1 : 0.9;
  QSize new_size = size() * factor;
  
  if (new_size.width() < 50 || new_size.height() < 50) {
      return;
  }

  if (new_size.width() > 5000 || new_size.height() > 5000) {
      return;
  }
  
  resize(new_size);
}

void PinnedWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        close();
    }
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
    QTimer::singleShot(100, this, &PinnedWindow::CheckHideUi);
    QWidget::leaveEvent(event);
}

void PinnedWindow::moveEvent(QMoveEvent* event) {
  QWidget::moveEvent(event);
  UpdateToolbarPosition();
}

void PinnedWindow::UpdateToolbarPosition() {
  if (!tool_bar_) return;
  
  int x = this->x() + this->width() - tool_bar_->width();
  int y = this->y() + this->height() + 5;
  
  tool_bar_->move(x, y);
}

void PinnedWindow::CheckHideUi() {
  if (!tool_bar_ || !tool_bar_->isVisible()) {
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
      QTimer::singleShot(100, this, &PinnedWindow::CheckHideUi);
    }
  }
  return QWidget::eventFilter(watched, event);
}

void PinnedWindow::OnCopy() {
    QClipboard* cb = QGuiApplication::clipboard();
    cb->setPixmap(pixmap_);
    
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

// call OCR
void PinnedWindow::OnOcr() {
    auto* engine = &OcrEngine::instance();
    
    auto* dlg = new OcrResultDialog(pixmap_, "Recognizing...", nullptr);
    dlg->show();
    dlg->raise();

    QPointer<OcrResultDialog> safeDlg(dlg);

    QTimer::singleShot(100, [engine, safeDlg, this]() {
        if (!safeDlg) return;

        QString result;
        try {
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
    bool visible = isVisible();
    
    if (is_pinned_) {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        if (btn_pin_) {
            btn_pin_->setChecked(true);
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

#include "OcrResultDialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QClipboard>
#include <QApplication>
#include <QSplitter>
#include <QTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QSizeGrip>
#include <QWheelEvent>
#include <QEnterEvent>

namespace {
    const char* kActionButtonStyle = R"(
  QToolButton {
    color: #555555;
    background: transparent;
    border-radius: 4px;
    padding: 4px 6px;
    border: 1px solid transparent;
  }
  QToolButton:hover {
    background: rgba(0, 0, 0, 0.06);
  }
  QToolButton:pressed {
    background: rgba(0, 0, 0, 0.1);
  }
)";
}

OcrResultDialog::OcrResultDialog(const QPixmap& pixmap, const QString& text, QWidget* parent)
    : QDialog(parent), pixmap_(pixmap), text_(text) {
    
    // 无边框，工具窗口，置顶
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    
    resize(800, 500);
    
    SetupUi();
}

void OcrResultDialog::SetupUi() {
    // 1. 关闭按钮 (右上角，绝对定位)
    close_btn_ = new QPushButton(this);
    close_btn_->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    close_btn_->setFixedSize(24, 24);
    close_btn_->setFlat(true);
    // 红色圆形背景样式
    close_btn_->setStyleSheet(
        "QPushButton { background-color: rgba(200, 0, 0, 0.5); border-radius: 12px; border: none; }"
        "QPushButton:hover { background-color: rgba(255, 0, 0, 0.8); }");
    close_btn_->setCursor(Qt::ArrowCursor);
    connect(close_btn_, &QPushButton::clicked, this, &QDialog::close);

    // 主布局
    auto* main_layout = new QVBoxLayout(this);
    // 顶部留出 32px 作为“标题栏”区域，避免遮挡关闭按钮
    main_layout->setContentsMargins(1, 32, 1, 1);
    main_layout->setSpacing(0);

    // 使用 Splitter 实现左右可调节布局
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1); // 细分割线
    splitter->setStyleSheet("QSplitter::handle { background-color: #E0E0E0; }");

    // 1. 左侧：图片显示区域
    scroll_area_ = new QScrollArea(splitter);
    scroll_area_->setWidgetResizable(false); // 手动控制大小以实现缩放
    scroll_area_->setAlignment(Qt::AlignCenter);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setStyleSheet("background-color: #F5F5F5;"); // 浅灰背景区分图片区
    
    image_label_ = new QLabel(scroll_area_);
    image_label_->setPixmap(pixmap_);
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setScaledContents(true); // 允许缩放内容
    scroll_area_->setWidget(image_label_);
    
    splitter->addWidget(scroll_area_);

    // 2. 右侧：文本显示区域
    auto* right_widget = new QWidget(splitter);
    right_widget->setStyleSheet("background-color: white;");
    auto* right_layout = new QVBoxLayout(right_widget);
    right_layout->setContentsMargins(10, 10, 10, 10);
    right_layout->setSpacing(10);

    // 文本编辑框
    text_edit_ = new QTextEdit(right_widget);
    text_edit_->setReadOnly(true);
    text_edit_->setPlainText(text_);
    text_edit_->setFont(QFont("Consolas", 10));
    text_edit_->setFrameShape(QFrame::NoFrame); // 无边框
    right_layout->addWidget(text_edit_);

    // 分隔线 (底部不需要按钮了，所以不需要分隔线和按钮栏)
    // 但是为了美观，可以留一个 padding
    
    splitter->addWidget(right_widget);
    
    // 设置初始比例 1:1
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    main_layout->addWidget(splitter);

    // 添加右下角调整大小的手柄
    auto* size_grip = new QSizeGrip(this);
    size_grip->resize(16, 16);
    size_grip->show();
    
    // --- 创建浮动工具栏 (Copy Button) ---
    tool_bar_ = new QWidget(this);
    tool_bar_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    tool_bar_->installEventFilter(this);
    tool_bar_->setAttribute(Qt::WA_StyledBackground);
    tool_bar_->setStyleSheet(
        "QWidget { background-color: rgba(245, 245, 245, 240); border: 1px solid #C8C8C8; border-radius: 6px; }");

    auto* tool_layout = new QHBoxLayout(tool_bar_);
    tool_layout->setContentsMargins(6, 4, 6, 4);
    tool_layout->setSpacing(2);

    // Copy 按钮
    btn_copy_ = new QToolButton(tool_bar_);
    btn_copy_->setText(tr("Copy"));
    btn_copy_->setToolTip("Copy Text to Clipboard");
    btn_copy_->setFixedSize(60, 28);
    btn_copy_->setAutoRaise(true);
    btn_copy_->setStyleSheet(kActionButtonStyle);
    connect(btn_copy_, &QToolButton::clicked, this, &OcrResultDialog::OnCopy);
    tool_layout->addWidget(btn_copy_);
    
    tool_bar_->adjustSize();
    tool_bar_->hide(); // 初始隐藏，enter 时显示
}

void OcrResultDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    
    // 更新右上角关闭按钮位置
    if (close_btn_) {
        close_btn_->move(width() - close_btn_->width() - 8, 8);
    }

    // 更新右下角 SizeGrip 位置
    QSizeGrip *grip = findChild<QSizeGrip*>();
    if (grip) {
        grip->move(width() - grip->width(), height() - grip->height());
    }
    
    UpdateToolbarPosition();
}

void OcrResultDialog::moveEvent(QMoveEvent* event) {
    QDialog::moveEvent(event);
    UpdateToolbarPosition();
}

void OcrResultDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    
    // 初始缩放：适应显示区域 (Fit View)
    if (scroll_area_ && !pixmap_.isNull()) {
        QSize area_size = scroll_area_->viewport()->size();
        // 如果 viewport 还没准备好，可能需要延迟
        if (area_size.width() > 0 && area_size.height() > 0) {
             double scale_w = (double)area_size.width() / pixmap_.width();
             double scale_h = (double)area_size.height() / pixmap_.height();
             // 选择较小的比例以完全展示
             scale_factor_ = std::min(scale_w, scale_h);
             // 如果图片本身很小，不要放大，保持 1.0 ? 
             // 用户说 "默认展示完整大小"，通常指 Fit View。
             // 如果 scale > 1.0 (图片比窗口小)，是否放大？ PinnedWindow 会铺满。
             // 这里我们保持比例缩放，最大不超过 1.0 可能是个好主意，但如果用户想看大图呢？
             // 暂时允许放大，或者限制 max(1.0, fit_scale)
             // 简单起见：Fit View
             if (scale_factor_ > 1.0) scale_factor_ = 1.0; // 默认不放大超过原图
             
             UpdateImageScale();
        }
    }
}

void OcrResultDialog::UpdateImageScale() {
    if (image_label_ && !pixmap_.isNull()) {
        QSize new_size = pixmap_.size() * scale_factor_;
        image_label_->resize(new_size);
    }
}

void OcrResultDialog::wheelEvent(QWheelEvent* event) {
    // 仅当鼠标在 scroll_area 区域内时才响应缩放
    if (scroll_area_ && scroll_area_->geometry().contains(event->position().toPoint())) {
        const int delta = event->angleDelta().y();
        if (delta == 0) return;

        double factor = (delta > 0) ? 1.1 : 0.9;
        scale_factor_ *= factor;
        
        // 限制缩放范围
        if (scale_factor_ < 0.1) scale_factor_ = 0.1;
        if (scale_factor_ > 5.0) scale_factor_ = 5.0;
        
        UpdateImageScale();
        event->accept();
    } else {
        QDialog::wheelEvent(event);
    }
}

void OcrResultDialog::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    // 绘制背景
    p.fillRect(rect(), Qt::white);

    // 绘制顶部“标题栏”区域背景 (浅灰)
    // 高度增加到 32
    QRect title_rect(0, 0, width(), 32);
    p.fillRect(title_rect, QColor(240, 240, 240));

    // 绘制边框
    p.setPen(QPen(QColor(0, 0, 0, 50), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void OcrResultDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 避免在点击按钮时触发拖拽
        if (close_btn_ && close_btn_->geometry().contains(event->pos())) {
            return;
        }
        
        // 仅允许在顶部标题栏区域拖拽 (32px)
        if (event->pos().y() <= 32) {
            dragging_ = true;
            drag_offset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        }
    }
}

void OcrResultDialog::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - drag_offset_);
    } else {
        dragging_ = false;
    }
}

void OcrResultDialog::closeEvent(QCloseEvent* event) {
    if (tool_bar_) tool_bar_->close();
    QDialog::closeEvent(event);
}

void OcrResultDialog::enterEvent(QEnterEvent* event) {
    if (close_btn_) close_btn_->show();
    if (tool_bar_) {
        UpdateToolbarPosition();
        tool_bar_->show();
    }
    QDialog::enterEvent(event);
}

void OcrResultDialog::leaveEvent(QEvent* event) {
    QTimer::singleShot(100, this, &OcrResultDialog::CheckHideUi);
    QDialog::leaveEvent(event);
}

void OcrResultDialog::UpdateToolbarPosition() {
    if (!tool_bar_) return;
    
    // 计算工具栏位置：主窗口右下角外部
    // 右对齐，下方 5px
    int x = this->x() + this->width() - tool_bar_->width();
    int y = this->y() + this->height() + 5;
    
    tool_bar_->move(x, y);
}

void OcrResultDialog::CheckHideUi() {
    if (!tool_bar_ || !tool_bar_->isVisible()) {
        // 如果鼠标也不在主窗口内，确保 close_btn 隐藏
        // 注意：close_btn 在窗口内，通常 enter/leave 会处理
        // 这里模仿 PinnedWindow，如果鼠标完全离开了，就隐藏
        if (!frameGeometry().contains(QCursor::pos())) {
             // 这里 PinnedWindow 的逻辑是：如果鼠标既不在 main 也不在 toolbar，则隐藏
        }
        return;
    }
    
    // 如果鼠标在主窗口内，不隐藏
    if (frameGeometry().contains(QCursor::pos())) return;
    
    // 如果鼠标在工具栏内，不隐藏
    if (tool_bar_->geometry().contains(QCursor::pos())) return;
    
    // 否则隐藏
    tool_bar_->hide();
    // close_btn_ 也可以选择隐藏，或者保持
    // PinnedWindow 是全隐藏
    // 这里 close_btn_ 在标题栏，比较常规，也许不用隐藏？
    // 但 PinnedWindow 隐藏了 close_btn。为了“一致性”，我们也隐藏吧。
    // 不过 PinnedWindow 的 close_btn 是浮动的。这里的是标题栏的一部分。
    // 还是保持显示吧，标题栏按钮通常不自动隐藏。
    // 但 tool_bar_ 必须隐藏。
}

void OcrResultDialog::SetResultText(const QString& text) {
    text_ = text;
    if (text_edit_) {
        text_edit_->setPlainText(text_);
    }
}

void OcrResultDialog::AppendResultText(const QString& text) {
    text_ += text;
    if (text_edit_) {
        text_edit_->append(text);
    }
}

void OcrResultDialog::OnCopy() {
    if (text_edit_) {
        QClipboard* cb = QApplication::clipboard();
        cb->setText(text_edit_->toPlainText());
        
        QString old_text = btn_copy_->text();
        btn_copy_->setText(tr("Copied!"));
        btn_copy_->setEnabled(false);
        
        QTimer::singleShot(1000, this, [this, old_text]() {
            if (btn_copy_) {
                btn_copy_->setText(old_text);
                btn_copy_->setEnabled(true);
            }
        });
    }
}

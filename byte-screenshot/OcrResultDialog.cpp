#include "OcrResultDialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QClipboard>
#include <QApplication>
#include <QTimer>
#include <QWheelEvent>
#include <QFrame>

OcrResultDialog::OcrResultDialog(const QPixmap& pixmap, const QString& text, QWidget* parent)
    : QDialog(parent), pixmap_(pixmap), text_(text) {
    
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    
    setWindowTitle("OCR Result");
    resize(1000, 680);

    // Dark theme style
    setStyleSheet(
        "QDialog {"
        "  background-color: #2b2b2b;"
        "  color: #f0f0f0;"
        "}"
        "QLabel {"
        "  color: #f0f0f0;"
        "}"
        "QTextEdit {"
        "  background-color: #3c3f41;"
        "  color: #f0f0f0;"
        "  border: 1px solid #555555;"
        "  selection-background-color: #5c9ded;"
        "}"
        "QFrame[frameShape=\"4\"] {"   // QFrame::HLine
        "  color: #555555;"
        "  background-color: #555555;"
        "}"
        "QScrollArea {"
        "  background-color: #2b2b2b;"
        "  border: none;"
        "}"
        "QPushButton {"
        "  background-color: #3c3f41;"
        "  color: #f0f0f0;"
        "  border-radius: 4px;"
        "  padding: 4px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #4b4f52;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #2f3336;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #2b2d2f;"
        "  color: #777777;"
        "}"
    );
    
    SetupUi();
}

void OcrResultDialog::SetupUi() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(10, 10, 10, 10);
    main_layout->setSpacing(10);

    // 图片和文本
    auto* center_layout = new QHBoxLayout();
    center_layout->setSpacing(10);

    // 分栏；左边区域放图片
    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(false); // We control widget size manually for zoom
    scroll_area_->setAlignment(Qt::AlignCenter);
    scroll_area_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    image_label_ = new QLabel(scroll_area_);
    image_label_->setPixmap(pixmap_);
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setScaledContents(true);
    scroll_area_->setWidget(image_label_);
    
    center_layout->addWidget(scroll_area_, 1);

    // 右边文本
    text_edit_ = new QTextEdit(this);
    text_edit_->setReadOnly(true);
    text_edit_->setPlainText(text_);
    text_edit_->setFont(QFont("Consolas", 10));
    text_edit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    center_layout->addWidget(text_edit_, 1);

    main_layout->addLayout(center_layout, 1);

    // 左右边界线
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    main_layout->addWidget(line);

    // 基础按钮
    auto* btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(10);
    btn_layout->addStretch();

    btn_copy_ = new QPushButton(tr("Copy"), this);
    connect(btn_copy_, &QPushButton::clicked, this, &OcrResultDialog::OnCopy);
    
    btn_close_ = new QPushButton(tr("Close"), this);
    connect(btn_close_, &QPushButton::clicked, this, &OcrResultDialog::close);

    btn_layout->addWidget(btn_copy_);
    btn_layout->addWidget(btn_close_);

    main_layout->addLayout(btn_layout);
}

void OcrResultDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    // Standard layout handles resizing
}

void OcrResultDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    
    // Initial fit view logic
    if (scroll_area_ && !pixmap_.isNull()) {
        QSize area_size = scroll_area_->viewport()->size();
        if (area_size.width() > 0 && area_size.height() > 0) {
             double scale_w = (double)area_size.width() / pixmap_.width();
             double scale_h = (double)area_size.height() / pixmap_.height();
             scale_factor_ = std::min(scale_w, scale_h);
             if (scale_factor_ > 1.0) scale_factor_ = 1.0;
             
             UpdateImageScale();
        }
    }
}

void OcrResultDialog::wheelEvent(QWheelEvent* event) {
    // Zoom support when hovering scroll area
    if (scroll_area_ && scroll_area_->geometry().contains(event->position().toPoint())) {
        const int delta = event->angleDelta().y();
        if (delta == 0) return;

        double factor = (delta > 0) ? 1.1 : 0.9;
        scale_factor_ *= factor;
        
        if (scale_factor_ < 0.1) scale_factor_ = 0.1;
        if (scale_factor_ > 5.0) scale_factor_ = 5.0;
        
        UpdateImageScale();
        event->accept();
    } else {
        QDialog::wheelEvent(event);
    }
}

void OcrResultDialog::UpdateImageScale() {
    if (image_label_ && !pixmap_.isNull()) {
        QSize new_size = pixmap_.size() * scale_factor_;
        image_label_->resize(new_size);
    }
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

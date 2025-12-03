#include "SecondaryToolBar.h"
#pragma execution_character_set("utf-8")
#include <QSlider>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QPainter>
#include <QPaintEvent>

SecondaryToolBar::SecondaryToolBar(QWidget* parent)
    : QWidget(parent) {
    setFixedHeight(40);
    setAttribute(Qt::WA_StyledBackground, true);
    BuildUi();
    ApplyCommonStyle();
}

void SecondaryToolBar::ApplyCommonStyle()
{
    // 整体浅灰圆角背景，和主工具栏风格接近
    setStyleSheet(
        "SecondaryToolBar {"
        "  background-color: rgba(245,245,245,240);"
        "  border-radius: 6px;"
        "}"
        "QLabel {"
        "  color: #444444;"
        "}"
        "QSlider::groove:horizontal {"
        "  height: 4px;"
        "  background: #dddddd;"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 12px;"
        "  height: 12px;"
        "  margin: -4px 0;"
        "  background: #ffffff;"
        "  border: 1px solid #bbbbbb;"
        "  border-radius: 6px;"
        "}"
    );
}

void SecondaryToolBar::BuildUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);

    // --- 左侧：粗细 + slider + 数值 ---
    /*widthLabel_ = new QLabel(QStringLiteral("粗细"), this);*/
    widthLabel_ = new QLabel(QStringLiteral("Thickness"), this); 
    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setRange(1, 40);
    slider_->setValue(strokeWidth_);
    slider_->setFixedWidth(150);

    valueLabel_ = new QLabel(QString::number(strokeWidth_), this);

    layout->addWidget(widthLabel_);
    layout->addWidget(slider_, 0);
    layout->addWidget(valueLabel_);
    layout->addSpacing(10);

    // 中间竖线分隔
    auto* sep = new QWidget(this);
    sep->setFixedWidth(1);
    sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    sep->setStyleSheet("background-color: #d0d0d0;");
    layout->addWidget(sep);
    layout->addSpacing(8);

    // --- 右侧：颜色文字 + 常用颜色 + 自定义颜色 ---
    colorPanel_ = new QWidget(this);
    auto* colorLayout = new QHBoxLayout(colorPanel_);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(4);

    colorText_ = new QLabel(QStringLiteral("color"), colorPanel_);
    colorLayout->addWidget(colorText_);

    // 常用颜色列表
    const QVector<QColor> presetColors = {
        QColor(255, 80, 80),    // 红
        QColor(255, 165, 0),    // 橙
        QColor(255, 215, 0),    // 黄
        QColor(0,   160, 0),    // 绿
        QColor(0,   160, 233),  // 蓝
        QColor(155, 89,  182),  // 紫
        QColor(0,   0,   0),    // 黑
        QColor(255, 255, 255)   // 白
    };

    for (const QColor& c : presetColors) {
        auto* btn = new QToolButton(colorPanel_);
        btn->setFixedSize(18, 18);
        btn->setAutoRaise(true);

        QString style = QString(
            "QToolButton {"
            "  background-color: %1;"
            "  border: 1px solid #666666;"
            "  border-radius: 3px;"
            "}"
            "QToolButton:hover {"
            "  border: 1px solid #333333;"
            "}"
        ).arg(c.name());
        btn->setStyleSheet(style);

        colorLayout->addWidget(btn);
        presetColorBtns_.push_back(btn);

        connect(btn, &QToolButton::clicked, this, [this, c]() {
            strokeColor_ = c;
            UpdateColorButton();
            emit StrokeColorChanged(strokeColor_);
            });
    }

    // 自定义颜色按钮（一个带边框的小色块）
    colorBtn_ = new QToolButton(colorPanel_);
    colorBtn_->setFixedSize(20, 20);
    colorBtn_->setAutoRaise(true);
    UpdateColorButton();
    colorLayout->addSpacing(6);
    colorLayout->addWidget(colorBtn_);

    layout->addWidget(colorPanel_);

    // --- 信号连接 ---
    // 粗细变化
    connect(slider_, &QSlider::valueChanged, this,
        [this](int v) {
            strokeWidth_ = v;
            if (valueLabel_) {
                valueLabel_->setText(QString::number(v));
            }
            emit StrokeWidthChanged(v);
        });

    // 自定义颜色选择
    connect(colorBtn_, &QToolButton::clicked, this,
        [this]() {
            QColor c = QColorDialog::getColor(
                strokeColor_, this,
                QStringLiteral("color"));
            if (!c.isValid()) return;
            strokeColor_ = c;
            UpdateColorButton();
            emit StrokeColorChanged(c);
        });
}

void SecondaryToolBar::UpdateColorButton() {
    if (!colorBtn_) return;

    QString style = QString(
        "QToolButton {"
        "  background-color: %1;"
        "  border: 1px solid #444444;"
        "  border-radius: 4px;"
        "}"
        "QToolButton:hover {"
        "  border: 1px solid #222222;"
        "}"
    ).arg(strokeColor_.name());

    colorBtn_->setStyleSheet(style);
}

void SecondaryToolBar::SetMode(Mode m) {
    mode_ = m;

    // StrokeMode：显示颜色；EraserMode：隐藏右侧颜色区域
    if (colorPanel_) {
        colorPanel_->setVisible(mode_ == StrokeMode);
    }
}

// 画一个圆角边框，让整个条看起来更像一个独立的小面板
void SecondaryToolBar::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect().adjusted(0, 0, -1, -1);
    p.setPen(QPen(QColor(200, 200, 200), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 6, 6);
}

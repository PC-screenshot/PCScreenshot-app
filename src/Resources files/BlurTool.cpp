#include "BlurTool.h"
#include <QPainter>
#include <QGraphicsBlurEffect>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

BlurTool::BlurTool(QObject* parent) : QObject(parent) {
}

void BlurTool::applyEffect(QPixmap& pixmap, const QRect& area, int opacity) {
    if (pixmap.isNull() || area.isEmpty()) {
        return;
    }

    // 限制作用区域
    QRect effectiveArea = area.intersected(pixmap.rect());
    if (effectiveArea.isEmpty()) return;

    // 创建临时图像用于模糊
    QPixmap temp = pixmap.copy(effectiveArea);

    // 使用QGraphicsEffect进行高斯模糊
    QGraphicsBlurEffect* blurEffect = new QGraphicsBlurEffect();
    blurEffect->setBlurRadius(15); // 固定模糊半径

    QGraphicsScene scene;
    QGraphicsPixmapItem item(temp);
    item.setGraphicsEffect(blurEffect);
    scene.addItem(&item);

    // 渲染模糊后的图像
    QPixmap blurred(temp.size());
    blurred.fill(Qt::transparent);
    QPainter painter(&blurred);
    scene.render(&painter);

    // 根据透明度混合
    opacity = qBound(0, opacity, 100);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(opacity / 100.0);

    // 将模糊结果绘制回原图
    QPainter mainPainter(&pixmap);
    mainPainter.drawPixmap(effectiveArea.topLeft(), blurred);
}

void BlurTool::setOpacity(int opacity) {
    opacity = qBound(0, opacity, 100);
    if (m_opacity != opacity) {
        m_opacity = opacity;
        emit opacityChanged(opacity);
    }
}

QWidget* BlurTool::createSettingsWidget(QWidget* parent) {
    auto* popup = new QWidget(parent, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_TranslucentBackground);

    auto* container = new QWidget(popup);
    container->setStyleSheet(
        "background: rgba(245, 245, 245, 240);"
        "border: 1px solid #cccccc;"
        "border-radius: 6px;"
    );

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);

    // 标题
    auto* titleLabel = new QLabel("diaphaneity", container);
    titleLabel->setStyleSheet("font-weight: bold; border: none;");
    layout->addWidget(titleLabel);

    // 滑动条区域
    auto* sliderLayout = new QHBoxLayout();
    sliderLayout->setSpacing(8);

    auto* slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(0, 100);
    slider->setValue(m_opacity);
    slider->setFixedWidth(200);

    auto* valueLabel = new QLabel(QString::number(m_opacity), container);
    valueLabel->setFixedWidth(30);
    valueLabel->setStyleSheet("border: none;");

    sliderLayout->addWidget(slider);
    sliderLayout->addWidget(valueLabel);
    layout->addLayout(sliderLayout);

    // 连接信号
    connect(slider, &QSlider::valueChanged, [this, valueLabel](int value) {
        setOpacity(value);
        valueLabel->setText(QString::number(value));
        });

    auto* mainLayout = new QVBoxLayout(popup);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(container);

    popup->setFixedSize(container->sizeHint() + QSize(20, 20));
    return popup;
}
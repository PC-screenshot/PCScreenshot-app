#include "MosaicTool.h"
#include <QPainter>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

MosaicTool::MosaicTool(QObject* parent) : QObject(parent) {
}

void MosaicTool::applyEffect(QPixmap& pixmap, const QRect& area, int blockSize) {
    if (pixmap.isNull() || area.isEmpty() || blockSize < 1) {
        return;
    }

    QPainter painter(&pixmap);
    QImage image = pixmap.toImage();

    // 限制作用区域在图像范围内
    QRect effectiveArea = area.intersected(pixmap.rect());

    for (int y = effectiveArea.top(); y < effectiveArea.bottom(); y += blockSize) {
        for (int x = effectiveArea.left(); x < effectiveArea.right(); x += blockSize) {
            QRect blockRect(x, y, blockSize, blockSize);
            blockRect = blockRect.intersected(effectiveArea);

            if (blockRect.isEmpty()) continue;

            // 计算块内平均颜色
            int r = 0, g = 0, b = 0, a = 0;
            int pixelCount = 0;

            for (int by = blockRect.top(); by < blockRect.bottom(); ++by) {
                for (int bx = blockRect.left(); bx < blockRect.right(); ++bx) {
                    QRgb pixel = image.pixel(bx, by);
                    r += qRed(pixel);
                    g += qGreen(pixel);
                    b += qBlue(pixel);
                    a += qAlpha(pixel);
                    pixelCount++;
                }
            }

            if (pixelCount > 0) {
                QColor avgColor(r / pixelCount, g / pixelCount,
                    b / pixelCount, a / pixelCount);
                painter.fillRect(blockRect, avgColor);
            }
        }
    }
}

void MosaicTool::setBlurLevel(int level) {
    level = qBound(5, level, 50);
    if (m_blurLevel != level) {
        m_blurLevel = level;
        emit blurLevelChanged(level);
    }
}

QWidget* MosaicTool::createSettingsWidget(QWidget* parent) {
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
    auto* titleLabel = new QLabel("Pixel", container);
    titleLabel->setStyleSheet("font-weight: bold; border: none;");
    layout->addWidget(titleLabel);

    // 滑动条区域
    auto* sliderLayout = new QHBoxLayout();
    sliderLayout->setSpacing(8);

    auto* slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(5, 50);
    slider->setValue(m_blurLevel);
    slider->setFixedWidth(200);

    auto* valueLabel = new QLabel(QString::number(m_blurLevel), container);
    valueLabel->setFixedWidth(30);
    valueLabel->setStyleSheet("border: none;");

    sliderLayout->addWidget(slider);
    sliderLayout->addWidget(valueLabel);
    layout->addLayout(sliderLayout);

    // 连接信号
    connect(slider, &QSlider::valueChanged, [this, valueLabel](int value) {
        setBlurLevel(value);
        valueLabel->setText(QString::number(value));
        });

    auto* mainLayout = new QVBoxLayout(popup);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(container);

    popup->setFixedSize(container->sizeHint() + QSize(20, 20));
    return popup;
}
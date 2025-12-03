#pragma once

#include <QObject>
#include <QPixmap>
#include <QWidget>

/**
 * @brief 高斯模糊效果工具类
 * @details 提供透明度可调节的模糊效果
 */
class BlurTool : public QObject {
    Q_OBJECT
public:
    explicit BlurTool(QObject* parent = nullptr);

    static void applyEffect(QPixmap& pixmap, const QRect& area, int opacity);

    QWidget* createSettingsWidget(QWidget* parent = nullptr);

    int opacity() const { return m_opacity; }
    void setOpacity(int opacity);

signals:
    void opacityChanged(int opacity);

private:
    int m_opacity = 50; // 透明度百分比，范围0-100
};
#pragma once

#include <QObject>
#include <QPixmap>
#include <QWidget>

/**
 * @brief 马赛克效果工具类
 * @details 提供马赛克效果应用和参数设置界面
 */
class MosaicTool : public QObject {
    Q_OBJECT
public:
    explicit MosaicTool(QObject* parent = nullptr);

    static void applyEffect(QPixmap& pixmap, const QRect& area, int blockSize);

    QWidget* createSettingsWidget(QWidget* parent = nullptr);

    int blurLevel() const { return m_blurLevel; }
    void setBlurLevel(int level);

signals:
    void blurLevelChanged(int level);

private:
    int m_blurLevel = 10; // 像素块大小，范围5-50
};
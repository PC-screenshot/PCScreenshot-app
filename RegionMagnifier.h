#pragma once

#include <QPoint>
#include <QRect>
#include <QPixmap>

class QPainter;

// 简单的区域放大镜：
// - 使用整屏截图作为 sourcePixmap
// - 鼠标当前位置用 widget 坐标传进来
// - paint 时自己算好采样和显示位置
class RegionMagnifier
{
public:
    RegionMagnifier();

    // 设置放大镜采样的源图像（一般就是整屏截图）
    // 注意：这里只保存指针，不接管生命周期，外部要保证 QPixmap 在使用期间有效
    void setSourcePixmap(const QPixmap* pixmap);

    // 更新鼠标位置（widget 坐标）
    void setCursorPos(const QPoint& pos);

    // 开关
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }

    // 设置放大镜的显示尺寸（像素）
    void setLensSize(int size);           // 默认 120
    // 设置放大倍数（整数）
    void setZoomFactor(int factor);       // 默认 4

    // 绘制放大镜
    // widgetRect 用来避免溢出到窗口外
    void paint(QPainter& painter, const QRect& widgetRect) const;

private:
    const QPixmap* source_ = nullptr;  // 不拥有
    QPoint cursor_pos_;               // widget 坐标
    bool enabled_ = true;
    int lens_size_ = 120;             // 放大镜“窗口”边长
    int zoom_factor_ = 4;             // 放大倍数
};

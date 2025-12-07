#pragma once

#include <QWidget>
#include <QColor>
#include <QVector>

class QSlider;
class QLabel;
class QToolButton;

// 形状 / 画笔 / 橡皮擦的二级工具栏：控制“粗细 + 颜色”
class SecondaryToolBar : public QWidget {
    Q_OBJECT
public:
    explicit SecondaryToolBar(QWidget* parent = nullptr);

    enum Mode {
        StrokeMode,   // Rect / Ellipse / Arrow / Pen：粗细 + 颜色
        EraserMode    // Eraser：只有粗细
    };

    void SetMode(Mode m);

    int strokeWidth() const { return strokeWidth_; }
    QColor strokeColor() const { return strokeColor_; }

signals:
    void StrokeWidthChanged(int w);
    void StrokeColorChanged(const QColor& c);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void BuildUi();
    void UpdateColorButton();
    void ApplyCommonStyle();

    // --- UI 控件 ---
    QSlider* slider_ = nullptr;
    QLabel* widthLabel_ = nullptr; // “粗细”
    QLabel* valueLabel_ = nullptr; // 粗细数值
    QWidget* colorPanel_ = nullptr; // 右侧颜色区域整体
    QLabel* colorText_ = nullptr; // “颜色”
    QToolButton* colorBtn_ = nullptr; // 自定义颜色按钮
    QVector<QToolButton*> presetColorBtns_; // 常用颜色按钮

    // --- 状态 ---
    Mode   mode_ = StrokeMode;
    int    strokeWidth_ = 4;
    QColor strokeColor_ = QColor(255, 80, 80); // 默认红色
};

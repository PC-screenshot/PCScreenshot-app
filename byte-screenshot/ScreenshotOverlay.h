#pragma once

#include <QWidget>
#include <QPixmap>
#include <QRect>
#include <QVector>
#include <QColor>
#include <QPointF>
class QWheelEvent;

#include "EditorToolbar.h"

#include "MosaicTool.h"
#include "BlurTool.h"

// 截图覆盖层：负责选区 + 编辑 + 工具栏 + 导出
class ScreenshotOverlay : public QWidget {
    Q_OBJECT

public:
    explicit ScreenshotOverlay(QWidget* parent = nullptr);

    // 设置整屏截图作为背景
    void SetBackground(const QPixmap& pixmap);

signals:
    // 目前我们主要是直接复制到剪贴板，
    // 这个信号可以留作以后需要把结果传回 MainWindow 使用。
    void RegionSelected(const QRect& rect);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // 阶段：选区阶段 vs 编辑阶段
    enum class Stage {
        kSelecting,
        kEditing,
    };

    // 当前绘图模式
    enum class DrawMode {
        kNone,
        kRect,
        kEllipse,
        kArrow,
        kMosaic,
        kBlur,
        // 以后可以追加 Pen / Text 等
    };

    bool InsideSelection(const QPoint& pos) const;
    void UpdateToolbarPosition();
    void OnToolSelected(EditorToolbar::Tool tool);

    void StartEditingIfNeeded();

    // 导出当前结果图像：优先返回编辑画布，否则原始选区
    QPixmap CurrentResultPixmap() const;

    // 工具处理
    void CopyResultToClipboard();
    void SaveToFile();
    void RunAiOcr();       // TODO: 打开 AI-OCR 窗口
    void RunLocalOcr();    // 本地 PaddleOCR
    void RunAiDescribe();  // TODO: 打开 AI 描述窗口
    void PinToDesktop();   // TODO: 固定到桌面

    // 撤销 / 重做
    void Undo();
    void Redo();

    // ---------- 成员变量 ----------

    Stage stage_ = Stage::kSelecting;
    DrawMode draw_mode_ = DrawMode::kNone;

    QPixmap background_;   // 整个屏幕截图
    QRect   selection_;    // 当前选区
    QPixmap canvas_;       // 选区内部绘制用的画布
    double zoom_scale_ = 1.0; // 选区内容缩放比例
    QPointF zoom_center_;
    QRect ComputeZoomSourceRect(const QSize& content_size) const;

    bool is_selecting_ = false;
    bool is_moving_ = false;
    bool is_drawing_ = false;
    bool modified_ = false;

    QPoint drag_offset_;       // 选区移动偏移
    QPoint edit_start_pos_;    // 编辑起点（画布坐标）
    QPoint edit_current_pos_;  // 编辑当前点（画布坐标）

    QColor current_color_ = QColor(255, 80, 80);  // 默认画笔颜色

    // 撤销 / 重做栈，存画布的快照
    QVector<QImage> undo_stack_;
    QVector<QImage> redo_stack_;

    EditorToolbar* toolbar_ = nullptr;

    // 工具实例
    MosaicTool* mosaicTool_;
    BlurTool* blurTool_;

    // 二级设置栏
    QWidget* mosaicPopup_;
    QWidget* blurPopup_;

    void showToolPopup(QWidget* popup);
};

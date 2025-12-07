#pragma once

#include <QWidget>
#include <QPixmap>
#include <QRect>
#include <QVector>
#include <QColor>
#include <QPointF>
#include <memory>

#include "AiDescribeDialog.h"
#include "EditorToolbar.h"
#include "MosaicTool.h"
#include "BlurTool.h"
#include "RegionMagnifier.h"
#include "SecondaryToolBar.h"

class QWheelEvent;

#ifdef Q_OS_WIN
#include "uiinspector.h"
#endif

// 截图覆盖层：负责选区 + 编辑 + 工具栏 + 导出 + 放大镜 + 自动窗口高亮
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
        kPen,      // 新增：画笔
        kEraser,   // 新增：橡皮擦
        // 以后可以追加 Text 等
    };

    // ---------- 新增：绘制对象 & 撤销状态 ----------
    struct DrawItem {
        enum class Type {
            kRect,
            kEllipse,
            kArrow,
            kPen,
            kMosaic,
            kBlur,
            kEraserFree,    // 自由像素擦除（恢复背景）
        } type = Type::kRect;

        QRect rect;                 // bounding box / area（相对于选区左上角）
        QPointF p1, p2;             // for arrow (start/end)
        QVector<QPoint> path;       // for pen / eraser free stroke (canvas-local coordinates)
        QColor color = QColor(255, 80, 80);
        int stroke_width = 4;
        int mosaic_level = 10;      // mosaic 参数
        int blur_opacity = 50;      // blur 参数
    };

    struct UndoState {
        QImage image;
        QVector<DrawItem> items;
    };
    // ---------------------------------------------

    bool InsideSelection(const QPoint& pos) const;
    void UpdateToolbarPosition();
    void UpdateSecondaryToolbarPosition();
    void OnToolSelected(EditorToolbar::Tool tool);

    void StartEditingIfNeeded();

    // 导出当前结果图像：优先返回编辑画布，否则原始选区
    QPixmap CurrentResultPixmap() const;

    // 工具处理
    void CopyResultToClipboard();
    void SaveToFile();
    void RunAiOcr();       // TODO: 打开 AI-OCR 窗口
    void RunAiDescribe();  // 打开 AI 描述窗口
    void PinToDesktop();   // 固定到桌面

    // 撤销 / 重做
    void Undo();
    void Redo();

    QRect ComputeZoomSourceRect(const QSize& content_size) const;
    void showToolPopup(QWidget* popup);

    // 新增：从 base + items 合成 canvas（preview 可选）
    void RepaintCanvasFromItems(const DrawItem* preview = nullptr);

    // hit-test：是否被橡皮命中（简单基于 bounding box / 距离）
    bool ItemHitTest(const DrawItem& item, const QVector<QPoint>& eraserPath, int eraserRadius) const;

    // ---------- 成员变量 ----------

    Stage    stage_ = Stage::kSelecting;
    DrawMode draw_mode_ = DrawMode::kNone;

    QPixmap background_;   // 整个屏幕截图
    QRect   selection_;    // 当前选区
    QPixmap canvas_;       // 选区内部绘制用的画布

    // 新增
    QPixmap base_pixmap_;                    // 进入编辑时保存的原始选区像素（canvas 的底）
    QVector<DrawItem> items_;                // 已提交的绘制对象
    std::unique_ptr<DrawItem> preview_item_; // 当前未提交的预览项（鼠标拖动时）

    double  zoom_scale_ = 1.0; // 选区内容缩放比例
    QPointF zoom_center_;

    bool is_selecting_ = false;
    bool is_moving_ = false;
    bool is_drawing_ = false;
    bool modified_ = false;

    QPoint drag_offset_;       // 选区移动偏移
    QPoint edit_start_pos_;    // 编辑起点（画布坐标）
    QPoint edit_current_pos_;  // 编辑当前点（画布坐标）

    QColor current_color_ = QColor(255, 80, 80);  // 默认画笔/形状颜色
    int    stroke_width_ = 4;                    // 形状/画笔/橡皮擦的粗细

    // 撤销 / 重做栈（保存 image + items）
    QVector<UndoState> undo_stack_;
    QVector<UndoState> redo_stack_;

    EditorToolbar* toolbar_ = nullptr;  // 顶部主工具栏
    SecondaryToolBar* sToolbar_ = nullptr;  // 形状/画笔/橡皮擦二级工具栏

    // 工具实例
    MosaicTool* mosaicTool_ = nullptr;
    BlurTool* blurTool_ = nullptr;

    // 二级设置栏（马赛克 / 模糊）
    QWidget* mosaicPopup_ = nullptr;
    QWidget* blurPopup_ = nullptr;

    // 放大镜
    QPoint          cursor_pos_;   // 当前鼠标位置（widget 坐标）
    RegionMagnifier magnifier_;

    // 橡皮模式：按住 Shift 切换为对象橡皮（默认自由像素擦除）
    bool eraser_object_mode_ = false;

#ifdef Q_OS_WIN
    // 自动窗口 / 控件识别（Hover 高亮）
    UIInspector ui_inspector_;
    QRect       hover_rect_;      // 高亮区域（widget 坐标）
    bool        hover_valid_ = false;
#endif
};
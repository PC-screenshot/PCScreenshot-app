#include "ScreenshotOverlay.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QColorDialog>
#include <QWheelEvent>
#include <QScreen>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QPointer>
#include <QShowEvent>

#include <QPainterPath>      // 新增：QPainterPath
#include <algorithm>         // 新增：std::min/std::max
#include <vector>            // 新增：std::vector
#include <cmath>             // 新增：std::hypot / std::cos / std::sin
#include <memory>            // 新增：std::unique_ptr
#include <QDebug>
#ifdef Q_OS_WIN
#include <windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif



class MosaicBlurController;
extern MosaicBlurController* g_mosaicBlurController;

ScreenshotOverlay::ScreenshotOverlay(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::Tool);
    setWindowState(Qt::WindowFullScreen);

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);  // 接受 ESC 键

    // 主工具栏作为子控件（不是顶层窗口），初始隐藏
    toolbar_ = new EditorToolbar(this);
    toolbar_->setFixedHeight(36);
    toolbar_->hide();

    connect(toolbar_, &EditorToolbar::ToolSelected,
        this, &ScreenshotOverlay::OnToolSelected);

    // 形状 / 画笔 / 橡皮擦 二级工具栏（粗细 + 颜色）
    sToolbar_ = new SecondaryToolBar(this);
    sToolbar_->hide();

    connect(sToolbar_, &SecondaryToolBar::StrokeWidthChanged,
        this, [this](int w) { stroke_width_ = w; });

    connect(sToolbar_, &SecondaryToolBar::StrokeColorChanged,
        this, [this](const QColor& c) { current_color_ = c; });

    // 初始化马赛克 / 模糊工具
    mosaicTool_ = new MosaicTool(this);
    blurTool_ = new BlurTool(this);

    // 创建马赛克 / 模糊的二级设置栏（初始隐藏）
    mosaicPopup_ = mosaicTool_->createSettingsWidget(this);
    mosaicPopup_->hide();

    blurPopup_ = blurTool_->createSettingsWidget(this);
    blurPopup_->hide();
    connect(sToolbar_, &SecondaryToolBar::EraserTypeChanged,
        this, [this](SecondaryToolBar::EraserType eraserType) {
            eraser_object_mode_ = (eraserType == SecondaryToolBar::EraserType::ObjectEraser);
        });

    // 放大镜：使用整屏截图作为源
    magnifier_.setSourcePixmap(&background_);
    magnifier_.setLensSize(120);  // 可调
    magnifier_.setZoomFactor(4);  // 可调
    magnifier_.setEnabled(true);
    hoverTimer_.setSingleShot(true);
    hoverTimer_.setInterval(30);  // 30ms 一次，大概 33 FPS，足够流畅
    connect(&hoverTimer_, &QTimer::timeout, this, [this]() {
        if (!isVisible()) return;
        if (stage_ != Stage::kSelecting) return;
        if (is_moving_ || is_selecting_) return;

        // 这里用上次鼠标位置做一次真正的 UIA 查询
        this->DoHoverInspect(pendingHoverPos_);
        });
    longShot_ = new LongShotCapture(this);
}

void ScreenshotOverlay::SetBackground(const QPixmap& pixmap)
{
    background_ = pixmap;
    // 再同步一次，防止外部在构造后才设置背景
    magnifier_.setSourcePixmap(&background_);
    update();
}

void ScreenshotOverlay::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);

    // 背景：原始屏幕截图
    painter.drawPixmap(0, 0, background_);

    // 全屏半透明遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 120));

#ifdef Q_OS_WIN
    // 选区为空 & 正在选区阶段时，画自动识别的窗口高亮
    if (stage_ == Stage::kSelecting &&
        selection_.isNull() &&
        hover_valid_)
    {
        painter.save();

        // 1. 把 hover 区域从遮罩里“挖出来”，显示原始截图
        if (!background_.isNull()) {
            QPixmap sub = background_.copy(hover_rect_);
            painter.drawPixmap(hover_rect_.topLeft(), sub);
        }

        // 2. 画和选区一样的边框
        painter.setPen(QPen(Qt::blue, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(hover_rect_);

        painter.restore();
    }
#endif

    QRect sel = selection_.normalized();

    if (!sel.isNull()) {
        painter.save();
        QRect target = sel;
        if (stage_ == Stage::kEditing && !canvas_.isNull()) {
            QRect src = ComputeZoomSourceRect(canvas_.size());
            painter.drawPixmap(target, canvas_, src);
        }
        else if (!background_.isNull()) {
            QPixmap sub = background_.copy(sel);
            QRect   src = ComputeZoomSourceRect(sub.size());
            painter.drawPixmap(target, sub, src);
        }
        painter.restore();

        painter.setPen(QPen(Qt::blue, 2));
        painter.drawRect(sel);
    }

    magnifier_.paint(painter, rect());

    // 让长截图模块在右侧画预览
    if (longShot_) {
        longShot_->paintPreview(painter, this->rect());
    }
    }

// 鼠标左键按下事件
void ScreenshotOverlay::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos = event->pos();
    if (stage_ == Stage::kSelecting) {
        // // 如果当前没有选区，并且点击在 hover 矩形里，
        // 先标记“可能是 hover 点击”，真正是否采用 hover_rect_ 等 mouseRelease 再决定
        hover_click_candidate_ =
            selection_.isNull() && hover_valid_ && hover_rect_.contains(pos);

        // 2. 下面保持你原来的逻辑：在已有选区内部拖动，或者拖拽创建新选区
        if (InsideSelection(pos)) {
            // 在已有选区内部 -> 开始移动
            is_moving_ = true;
            drag_offset_ = pos - selection_.topLeft();
            toolbar_->hide();
            if (sToolbar_) sToolbar_->hide();
        }
        else {
            // 新建选区（拖拽）
            is_selecting_ = true;
            selection_ = QRect(pos, pos);
            toolbar_->hide();
            if (sToolbar_) sToolbar_->hide();
        }
        update();
        return;
    }

    // 编辑阶段：在选区内开始绘制
     if (stage_ == Stage::kEditing) {
        if (!selection_.contains(pos)) {
            return;
        }

        QPoint local = pos - selection_.topLeft();

        if (draw_mode_ != DrawMode::kNone) {
            // 在绘制前保存快照，供 Undo 使用（保存 image + items）
            if (!canvas_.isNull()) {
                UndoState st;
                st.image = canvas_.toImage();
                st.items = items_;
                undo_stack_.push_back(std::move(st));
                redo_stack_.clear();
            }

            is_drawing_ = true;
            edit_start_pos_ = local;
            edit_current_pos_ = local;

            // 新建 preview_item_
            preview_item_.reset(new DrawItem());
            preview_item_->color = current_color_;
            preview_item_->stroke_width = stroke_width_;

            switch (draw_mode_) {
            case DrawMode::kRect:
                preview_item_->type = DrawItem::Type::kRect;
                preview_item_->rect = QRect(edit_start_pos_, edit_start_pos_);
                break;
            case DrawMode::kEllipse:
                preview_item_->type = DrawItem::Type::kEllipse;
                preview_item_->rect = QRect(edit_start_pos_, edit_start_pos_);
                break;
            case DrawMode::kArrow:
                preview_item_->type = DrawItem::Type::kArrow;
                preview_item_->p1 = edit_start_pos_;
                preview_item_->p2 = edit_start_pos_;
                break;
            case DrawMode::kPen:
                preview_item_->type = DrawItem::Type::kPen;
                preview_item_->path.clear();
                preview_item_->path.append(edit_start_pos_);
                break;
            case DrawMode::kEraser:
                // 按住 Shift 切换为对象橡皮
                eraser_object_mode_ = (event->modifiers() & Qt::ShiftModifier);
                if (eraser_object_mode_) {
                    // 预览路径；真正删除在 mouseRelease 里
                    preview_item_->type = DrawItem::Type::kPen; // 复用 path
                    preview_item_->path.clear();
                    preview_item_->path.append(edit_start_pos_);
                } else {
                    preview_item_->type = DrawItem::Type::kEraserFree;
                    preview_item_->path.clear();
                    preview_item_->path.append(edit_start_pos_);
                    preview_item_->stroke_width = stroke_width_;
                }
                break;
            case DrawMode::kMosaic:
                preview_item_->type = DrawItem::Type::kMosaic;
                preview_item_->rect = QRect(edit_start_pos_, edit_start_pos_);
                preview_item_->mosaic_level = mosaicTool_ ? mosaicTool_->blurLevel() : 10;
                break;
            case DrawMode::kBlur:
                preview_item_->type = DrawItem::Type::kBlur;
                preview_item_->rect = QRect(edit_start_pos_, edit_start_pos_);
                preview_item_->blur_opacity = blurTool_ ? blurTool_->opacity() : 50;
                break;
            default:
                break;
            }

            // 即时绘制预览
            RepaintCanvasFromItems(preview_item_.get());
        }
    }
}

// 鼠标移动事件
void ScreenshotOverlay::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->pos();
    cursor_pos_ = pos;                 // 放大镜用
    // ---- 放大镜逻辑：如果已经有选区，则只在选区内启用放大镜 ----
    bool hasSelection = !selection_.isNull();
    if (hasSelection && (stage_ == Stage::kSelecting || stage_ == Stage::kEditing)) {
        // 已经确定了选区（或正在编辑）：只在选区区域内显示放大镜
        if (selection_.normalized().contains(pos)) {
            magnifier_.setEnabled(true);
            magnifier_.setCursorPos(pos);
        }
        else {
            // 鼠标移出选区 -> 关闭放大镜
            magnifier_.setEnabled(false);
        }
    }
    else {
        // 还没有选区时：保持原来的行为，全屏跟随鼠标
        magnifier_.setEnabled(true);
        magnifier_.setCursorPos(pos);
    }
    if (stage_ == Stage::kSelecting) {
        if (is_moving_) {
            QPoint new_top_left = pos - drag_offset_;
            selection_.moveTo(new_top_left);
            if (toolbar_->isVisible()) {
                UpdateToolbarPosition();
                UpdateSecondaryToolbarPosition();
            }
        }
        else if (is_selecting_) {
            selection_.setBottomRight(pos);
#ifdef Q_OS_WIN
            // 如果用户拖出了一个“比较大的”矩形，就不再认为这是单击 hover 了
            if (hover_click_candidate_) {
                if (selection_.width() > 3 || selection_.height() > 3) {
                    hover_click_candidate_ = false;
                }
            }
#endif
        }
        else {
            // 没在拉选区、也没在拖动：做自动窗口识别（Hover 高亮）
            pendingHoverPos_ = pos;
            if (!hoverTimer_.isActive()) {
                hoverTimer_.start();
            }


        }
        update();
        return;
    }

    if(stage_ == Stage::kEditing && is_drawing_) {
        edit_current_pos_ = pos - selection_.topLeft();

        if(preview_item_) {
            switch (preview_item_->type) {
            case DrawItem::Type::kRect:
            case DrawItem::Type::kEllipse:
            case DrawItem::Type::kMosaic:
            case DrawItem::Type::kBlur:
                preview_item_->rect = QRect(edit_start_pos_, edit_current_pos_).normalized();
                break;
            case DrawItem::Type::kArrow:
                preview_item_->p2 = edit_current_pos_;
                preview_item_->rect = QRect(preview_item_->p1.toPoint(), preview_item_->p2.toPoint()).normalized();
                break;
            case DrawItem::Type::kPen:
                // append if movement significant
                if (preview_item_->path.isEmpty() || (preview_item_->path.last() - edit_current_pos_).manhattanLength() > 1) {
                    preview_item_->path.append(edit_current_pos_);
                }
                break;
            case DrawItem::Type::kEraserFree:
                if (preview_item_->path.isEmpty() || (preview_item_->path.last() - edit_current_pos_).manhattanLength() > 1) {
                    preview_item_->path.append(edit_current_pos_);
                }
                break;
            default:
                break;
            }
        }

        RepaintCanvasFromItems(preview_item_.get());
        return;
    }

    // 其它阶段也重绘一下（放大镜位置会变）
    update();
}

// 鼠标左键释放事件
void ScreenshotOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);

    // ---------- 选区阶段 ----------
    if (stage_ == Stage::kSelecting) {

        // 结束“拖拽新建选区”
        if (is_selecting_) {
            is_selecting_ = false;
            selection_ = selection_.normalized();

            if (selection_.width() < 5 || selection_.height() < 5) {
                // 认为这是“点击”，不是拖出一个有效矩形
#ifdef Q_OS_WIN
                if (hover_click_candidate_ && hover_valid_) {
                    // ✅ 把 hover 矩形作为真正的选区
                    selection_ = hover_rect_.normalized();

                    if (!selection_.isNull()) {
                        toolbar_->show();
                        UpdateToolbarPosition();
                        UpdateSecondaryToolbarPosition();
                        zoom_scale_ = 1.0;
                        zoom_center_ = QPointF(selection_.width() / 2.0,
                            selection_.height() / 2.0);
                    }
                }
                else
#endif
                {
                    // 普通点击空白或非 hover 区域：清空选区
                    selection_ = QRect();
                    toolbar_->hide();
                    if (sToolbar_) sToolbar_->hide();
                }
            }
            else {
                // 正常拖出来的有效选区
                toolbar_->show();
                UpdateToolbarPosition();
                UpdateSecondaryToolbarPosition();
                zoom_scale_ = 1.0;
                zoom_center_ = QPointF(selection_.width() / 2.0,
                    selection_.height() / 2.0);
            }
        }

        // 结束“拖动已有选区移动”
        if (is_moving_) {
            is_moving_ = false;

            if (!selection_.isNull()) {
                toolbar_->show();
                UpdateToolbarPosition();
                UpdateSecondaryToolbarPosition();
                zoom_center_ = QPointF(selection_.width() / 2.0,
                    selection_.height() / 2.0);
            }
        }

#ifdef Q_OS_WIN
        // 一次按键周期结束，重置 hover 点击标记
        hover_click_candidate_ = false;
#endif
        update();
        return;
    }

    // ---------- 编辑阶段：结束绘制（Rect / Ellipse / Arrow / Mosaic / Blur） ----------
    if (stage_ == Stage::kEditing && is_drawing_) {
        is_drawing_ = false;

        if (canvas_.isNull()) {
            return;
        }

        if (!preview_item_) return;

        // 完成预览并把 preview_item_ 转为正式 item（或执行对象擦除）
        if (preview_item_->type == DrawItem::Type::kEraserFree) {
            // 自由像素擦除：把一个 EraserFree item push 到 items_
            DrawItem itm = *preview_item_;
            items_.push_back(itm);
        }
        else {
            // 对象橡皮模式（Shift）: 删除被擦除对象
            if (draw_mode_ == DrawMode::kEraser && eraser_object_mode_) {
                // preview_item_->path 存储了擦除轨迹
                QVector<QPoint> erasePath = preview_item_->path;
                int radius = preview_item_->stroke_width;
                QVector<int> removeIdx;
                for (int i = 0; i < items_.size(); ++i) {
                    if (ItemHitTest(items_[i], erasePath, radius)) {
                        removeIdx.append(i);
                    }
                }
                // 从后往前删除
                for (int i = removeIdx.size() - 1; i >= 0; --i) {
                    items_.remove(removeIdx[i]);
                }
            }
            else {
                // 普通对象：rect/ellipse/arrow/pen/mosaic/blur
                DrawItem itm = *preview_item_;
                // normalize rect if needed
                if (!itm.rect.isNull()) itm.rect = itm.rect.normalized();
                items_.push_back(itm);
            }
        }

        // 清除 preview 并重绘 canvas
        preview_item_.reset();
        RepaintCanvasFromItems(nullptr);

        modified_ = true;
        update();
    }
}


bool ScreenshotOverlay::InsideSelection(const QPoint& pos) const
{
    return selection_.normalized().contains(pos);
}

// 键盘事件
void ScreenshotOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
    }
}
void ScreenshotOverlay::wheelEvent(QWheelEvent* event)
{
    bool longShotActive = (longShot_ && longShot_->isActive());
    qDebug() << "[Overlay] wheelEvent: delta =" << event->angleDelta()
        << "pos =" << event->position()
        << "global =" << event->globalPosition()
        << "longShotActive =" << longShotActive
        << "hasFocus =" << hasFocus();

    if (longShotActive) {
        // 在 QQ 模式下，理论上 Overlay 不应该再收到滚轮
        // 如果还是打印到这里，说明消息仍然发给了 Overlay
        // 这里我们既不调用基类，也不 accept，直接 return，
        // 反正底层已经收不到这条消息了。
        return;
    }

    QWidget::wheelEvent(event);
}
    

//void ScreenshotOverlay::wheelEvent(QWheelEvent* event)
//{
//    if (selection_.isNull()) {
//        return;
//    }
//    const int delta = event->angleDelta().y();
//    if (delta == 0) return;
//    QPoint p = event->position().toPoint();
//    if (selection_.contains(p)) {
//        QPoint local = p - selection_.topLeft();
//        zoom_center_ = QPointF(local);
//    }
//    double factor = (delta > 0) ? 1.1 : 0.9;
//    zoom_scale_ *= factor;
//    if (zoom_scale_ < 0.2) zoom_scale_ = 0.2;
//    if (zoom_scale_ > 8.0) zoom_scale_ = 8.0;
//    update();
//}

QRect ScreenshotOverlay::ComputeZoomSourceRect(const QSize& content_size) const
{
    if (content_size.isEmpty()) return QRect();
    double w = content_size.width() / zoom_scale_;
    double h = content_size.height() / zoom_scale_;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    QPointF center = zoom_center_;
    if (center.isNull()) {
        center = QPointF(content_size.width() / 2.0, content_size.height() / 2.0);
    }
    double x = center.x() - w / 2.0;
    double y = center.y() - h / 2.0;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > content_size.width()) x = content_size.width() - w;
    if (y + h > content_size.height()) y = content_size.height() - h;
    return QRect(int(x), int(y), int(w), int(h));
}

void ScreenshotOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (toolbar_ && toolbar_->isVisible() && !selection_.isNull()) {
        UpdateToolbarPosition();
        UpdateSecondaryToolbarPosition();
    }
}

// 工具栏位置：选区下方，右边缘对齐选区右边缘
void ScreenshotOverlay::UpdateToolbarPosition()
{
    if (!toolbar_ || selection_.isNull()) {
        return;
    }

    const QSize tb_size = toolbar_->sizeHint();
    const int   w = tb_size.width();
    const int   h = toolbar_->height() > 0 ? toolbar_->height() : tb_size.height();

    int x = selection_.right() - w;
    int y = selection_.bottom() + 8;

    if (x < 10) {
        x = 10;
    }
    if (x + w > width() - 10) {
        x = width() - w - 10;
    }

    if (y + h > height() - 10) {
        y = selection_.top() - h - 8;
        if (y < 10) {
            y = height() - h - 10;
        }
    }

    toolbar_->setGeometry(x, y, w, h);
}
void ScreenshotOverlay::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // 把自己激活并获取键盘焦点
    activateWindow();
    raise();
    if (!longShot_ || !longShot_->isActive()) {
        setFocus(Qt::ActiveWindowFocusReason);
    }
    qDebug() << "[Overlay] showEvent, hasFocus =" << hasFocus();
}
// 二级工具栏位置
void ScreenshotOverlay::UpdateSecondaryToolbarPosition()
{
    if (!sToolbar_ || !toolbar_ || !sToolbar_->isVisible()) {
        return;
    }

    // 一级工具栏左上角的全局坐标
    QPoint tbPosGlobal = toolbar_->mapToGlobal(QPoint(0, 0));

    // 和一级工具栏左侧对齐
    int x = tbPosGlobal.x();
    int y = tbPosGlobal.y() + toolbar_->height() + 6;  // 在下方 6px

    // 防止出屏
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect sr = screen->availableGeometry();

        if (x < sr.left())
            x = sr.left();
        if (x + sToolbar_->width() > sr.right())
            x = sr.right() - sToolbar_->width();

        if (y + sToolbar_->height() > sr.bottom()) {
            // 如果底部放不下，就放到工具栏上方
            y = tbPosGlobal.y() - sToolbar_->height() - 6;
        }
    }

    sToolbar_->move(x, y);
}

void ScreenshotOverlay::StartEditingIfNeeded()
{
    if (stage_ == Stage::kEditing) {
        return;
    }
    if (selection_.isNull() || background_.isNull()) {
        return;
    }

    // 先把选区剪成 base_pixmap_，后续所有绘制都叠加在这个基础图上
    base_pixmap_ = background_.copy(selection_);
    canvas_ = base_pixmap_;

    modified_ = false;
    undo_stack_.clear();
    redo_stack_.clear();
    items_.clear();
    preview_item_.reset();

    stage_ = Stage::kEditing;
}

//得到现在的结果图像：优先返回编辑画布，否则原始选区
QPixmap ScreenshotOverlay::CurrentResultPixmap() const
{
    QRect sel = selection_.normalized();

    if (stage_ == Stage::kEditing && !canvas_.isNull()) {
        return canvas_;
    }
    if (!sel.isNull() && !background_.isNull()) {
        return background_.copy(sel);
    }
    return QPixmap();
}
//---各个工具的实现
//复制到剪切板
void ScreenshotOverlay::CopyResultToClipboard()
{
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }
    QClipboard* cb = QGuiApplication::clipboard();
    cb->setPixmap(result);
}

//保存到本地
void ScreenshotOverlay::SaveToFile()
{
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }

    const QString timestamp =
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");

    const QString default_file_name =
        QString("qtscreenshot-%1.png").arg(timestamp);

    QString default_dir =
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (default_dir.isEmpty()) {
        default_dir = QDir::currentPath();
    }

    const QString default_path =
        QDir(default_dir).filePath(default_file_name);

    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Screenshot"),
        default_path,
        tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;Bitmap (*.bmp)"));

    if (path.isEmpty()) {
        return;
    }

    result.save(path);
}
// 本地 PaddleOCR
void ScreenshotOverlay::RunLocalOcr() {
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }

    // 创建并显示结果对话框 (Parent设为nullptr以独立于Overlay)
    auto* dlg = new OcrResultDialog(result, "Recognizing...", nullptr);
    dlg->move(this->geometry().center() - dlg->rect().center());
    dlg->show();

    // 关闭截图区域
    close();

    // 异步执行 OCR，避免阻塞 UI 关闭
    // 使用 QPointer 确保安全
    QPointer<OcrResultDialog> safeDlg(dlg);

    QTimer::singleShot(100, [result, safeDlg]() {
        if (!safeDlg) return;

        auto* engine = &OcrEngine::instance();
        QString text;
        try {
            // 同步调用
            text = engine->detectText(result.toImage());
        }
        catch (const std::exception& e) {
            text = QString("Error: %1").arg(e.what());
        }
        catch (...) {
            text = "Unknown Error during OCR dispatch.";
        }

        if (safeDlg) {
            safeDlg->SetResultText(text);
        }
        });
}

//AI 描述
void ScreenshotOverlay::RunAiDescribe()
{
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }

    // 弹出 AI 描述窗口
    auto* dlg = new AiDescribeDialog(result, nullptr);

    // 设置 API Key / 模型（也可以内部用环境变量）
    dlg->setApiKey("284143f6-2e1b-42a1-8acb-82007ebe0c1d");
    dlg->setModel("doubao-seed-1-6-flash-250828");

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    close();
}

//pin
void ScreenshotOverlay::PinToDesktop()
{
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }
    auto* pin = PinnedWindow::CreatePinnedWindow(result, nullptr);
    if (pin) {
        pin->setOcrEnabled(true);
    }
}

void ScreenshotOverlay::Undo()
{
    if (undo_stack_.isEmpty() || canvas_.isNull()) {
        return;
    }
    // 当前状态推到 redo
    {
        UndoState cur;
        cur.image = canvas_.toImage();
        cur.items = items_;
        redo_stack_.push_back(std::move(cur));
    }
    // 弹出一个 undo 状态恢复
    UndoState st = undo_stack_.takeLast();
    canvas_ = QPixmap::fromImage(st.image);
    items_ = st.items;
    modified_ = true;
    update();
}


void ScreenshotOverlay::Redo()
{
    if (redo_stack_.isEmpty() || canvas_.isNull()) {
        return;
    }
    // 当前状态推到 undo
    {
        UndoState cur;
        cur.image = canvas_.toImage();
        cur.items = items_;
        undo_stack_.push_back(std::move(cur));
    }
    // 弹出一个 redo 状态恢复
    UndoState st = redo_stack_.takeLast();
    canvas_ = QPixmap::fromImage(st.image);
    items_ = st.items;
    modified_ = true;
    update();
}


void ScreenshotOverlay::OnToolSelected(EditorToolbar::Tool tool)
{
    // 切换工具前先把其它 popup 收起来
    if (mosaicPopup_) mosaicPopup_->hide();
    if (blurPopup_)   blurPopup_->hide();
    if (sToolbar_)    sToolbar_->hide();

    switch (tool) {
        // 绘图工具：固定选区，进入编辑阶段 + 切换模式
    case EditorToolbar::Tool::kRect:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kRect;
        if (sToolbar_) {
            sToolbar_->SetMode(SecondaryToolBar::StrokeMode);
            sToolbar_->show();
            UpdateSecondaryToolbarPosition();
        }
        break;

    case EditorToolbar::Tool::kEllipse:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kEllipse;
        if (sToolbar_) {
            sToolbar_->SetMode(SecondaryToolBar::StrokeMode);
            sToolbar_->show();
            UpdateSecondaryToolbarPosition();
        }
        break;

    case EditorToolbar::Tool::kArrow:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kArrow;
        if (sToolbar_) {
            sToolbar_->SetMode(SecondaryToolBar::StrokeMode);
            sToolbar_->show();
            UpdateSecondaryToolbarPosition();
        }
        break;

    case EditorToolbar::Tool::kPen:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kPen;
        if (sToolbar_) {
            sToolbar_->SetMode(SecondaryToolBar::StrokeMode);
            sToolbar_->show();
            UpdateSecondaryToolbarPosition();
        }
        break;

    case EditorToolbar::Tool::kEraser:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kEraser;
        if (sToolbar_) {
            sToolbar_->SetMode(SecondaryToolBar::EraserMode);
            sToolbar_->show();
            UpdateSecondaryToolbarPosition();
        }
        break;

    case EditorToolbar::Tool::kMosaic:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kMosaic;
        // 显示马赛克设置栏
        showToolPopup(mosaicPopup_);
        break;

    case EditorToolbar::Tool::kBlur:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kBlur;
        // 显示模糊设置栏
        showToolPopup(blurPopup_);
        break;

    case EditorToolbar::Tool::kUndo:
        Undo();
        break;
    case EditorToolbar::Tool::kRedo:
        Redo();
        break;
    case EditorToolbar::Tool::kOcr: {
        StartEditingIfNeeded();
        RunLocalOcr();
        break;
    }
    case EditorToolbar::Tool::kAiDescribe:
        StartEditingIfNeeded();
        RunAiDescribe();
        break;

    case EditorToolbar::Tool::kLongShot:
    qDebug() << "[Overlay] kLongShot clicked, selection =" << selection_;
    if (!selection_.isNull() && longShot_) {
        qDebug() << "[Overlay] start longShot with selection (normalized) =" << selection_.normalized();
        longShot_->start(selection_.normalized(), this);
        qDebug() << "[Overlay] longShot active =" << longShot_->isActive();

#ifdef Q_OS_WIN
        // 额外一步：把焦点交回底层窗口（VS / 浏览器等）
        HWND hwndTarget = longShot_->debugTargetWindow();
        qDebug() << "[Overlay] debugTargetWindow =" << hwndTarget;
        if (hwndTarget) {
            SetForegroundWindow(hwndTarget);
            SetFocus(hwndTarget);
            qDebug() << "[Overlay] SetForegroundWindow & SetFocus to targetWindow";
        }
#endif
        // 自己尽量不要再获取键盘焦点
        setFocusPolicy(Qt::NoFocus);
        clearFocus();
        qDebug() << "[Overlay] focus policy set to NoFocus, hasFocus =" << hasFocus();
    }
    else {
        qDebug() << "[Overlay] longShot NOT started, selection_.isNull() ="
            << selection_.isNull() << ", longShot_ =" << longShot_;
    }
    break;


    case EditorToolbar::Tool::kPin:
        StartEditingIfNeeded();
        PinToDesktop();
        close();
        break;

    case EditorToolbar::Tool::kSave:
        StartEditingIfNeeded();
        SaveToFile();
        close();
        break;

    case EditorToolbar::Tool::kDone:
        StartEditingIfNeeded();
        CopyResultToClipboard();
        close();
        break;

    case EditorToolbar::Tool::kCancel:
        close();
        break;

    default:
        break;
    }
}

void ScreenshotOverlay::showToolPopup(QWidget* popup)
{
    if (!popup || !toolbar_) return;

    // 获取工具栏在主屏幕的位置
    QPoint toolbarPos = toolbar_->mapToGlobal(QPoint(0, 0));
    QRect  toolbarRect = toolbar_->rect();

    // 计算弹出位置：工具栏下方中央
    int x = toolbarPos.x() + (toolbarRect.width() - popup->width()) / 2;
    int y = toolbarPos.y() + toolbarRect.height() + 8;

    // 确保不超出屏幕边界
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        if (x + popup->width() > screenRect.right()) {
            x = screenRect.right() - popup->width();
        }
        if (x < screenRect.left()) {
            x = screenRect.left();
        }
        if (y + popup->height() > screenRect.bottom()) {
            y = toolbarPos.y() - popup->height() - 8;
        }
    }

    popup->move(x, y);
    popup->show();
    popup->raise();
}
//// ---------- 合成渲染：从 base_pixmap_ + items_ 重新生成 canvas_ ----------
//void ScreenshotOverlay::RepaintCanvasFromItems(const DrawItem* preview /*= nullptr*/)
//{
//    if (base_pixmap_.isNull())
//        return;
//
//    // 1. 以基础截图为起点
//    canvas_ = base_pixmap_.copy();
//
//    // --------- 1) 先应用所有像素级效果（马赛克 / 模糊 / 自由橡皮）---------
//    auto applyPixelItem = [&](const DrawItem& it) {
//        switch (it.type) {
//        case DrawItem::Type::kMosaic:
//            if (mosaicTool_) {
//                // rect 是选区内局部坐标，正好对应 canvas_
//                MosaicTool::applyEffect(canvas_, it.rect, it.mosaic_level);
//            }
//            break;
//        case DrawItem::Type::kBlur:
//            if (blurTool_) {
//                BlurTool::applyEffect(canvas_, it.rect, it.blur_opacity);
//            }
//            break;
//        case DrawItem::Type::kEraserFree: {
//            // 将 base_pixmap_ 上对应区域复制回 canvas_（自由橡皮）
//            QImage baseImg = base_pixmap_.toImage();
//            QImage dstImg = canvas_.toImage();
//            int radius = it.stroke_width / 2;
//
//            for (const QPoint& pt : it.path) {
//                int cx = pt.x();
//                int cy = pt.y();
//                int x0 = std::max(0, cx - radius);
//                int y0 = std::max(0, cy - radius);
//                int x1 = std::min(baseImg.width() - 1, cx + radius);
//                int y1 = std::min(baseImg.height() - 1, cy + radius);
//                for (int y = y0; y <= y1; ++y) {
//                    for (int x = x0; x <= x1; ++x) {
//                        int dx = x - cx;
//                        int dy = y - cy;
//                        if (dx * dx + dy * dy <= radius * radius) {
//                            dstImg.setPixel(x, y, baseImg.pixel(x, y));
//                        }
//                    }
//                }
//            }
//            canvas_ = QPixmap::fromImage(dstImg);
//            break;
//        }
//        default:
//            // 其它类型（矩形/椭圆/箭头/画笔）不在像素阶段处理
//            break;
//        }
//        };
//
//    for (const DrawItem& it : items_) {
//        applyPixelItem(it);
//    }
//    if (preview) {
//        applyPixelItem(*preview);
//    }
//
//    // --------- 2) 再画所有矢量对象（矩形、圆、箭头、画笔）---------
//    QPainter p(&canvas_);
//    p.setRenderHint(QPainter::Antialiasing, true);
//
//    auto drawVectorItem = [&](const DrawItem& it) {
//        switch (it.type) {
//        case DrawItem::Type::kRect: {
//            QPen pen(it.color, it.stroke_width,
//                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
//            p.setPen(pen);
//            p.setBrush(Qt::NoBrush);
//            p.drawRect(it.rect);
//            break;
//        }
//        case DrawItem::Type::kEllipse: {
//            QPen pen(it.color, it.stroke_width,
//                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
//            p.setPen(pen);
//            p.setBrush(Qt::NoBrush);
//            p.drawEllipse(it.rect);
//            break;
//        }
//        case DrawItem::Type::kArrow: {
//            QPen pen(it.color, it.stroke_width,
//                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
//            p.setPen(pen);
//            QPointF a = it.p1, b = it.p2;
//            p.drawLine(a, b);
//
//            // 画箭头
//            QPointF dir = a - b;
//            double len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
//            if (len > 0.1) {
//                double ux = dir.x() / len;
//                double uy = dir.y() / len;
//                double hl = std::max(10.0, it.stroke_width * 3.0);
//                double angle = M_PI / 6.0; // 30 度
//
//                QPointF p1 = b + QPointF(
//                    ux * hl * std::cos(angle) - uy * hl * std::sin(angle),
//                    ux * hl * std::sin(angle) + uy * hl * std::cos(angle));
//                QPointF p2 = b + QPointF(
//                    ux * hl * std::cos(-angle) - uy * hl * std::sin(-angle),
//                    ux * hl * std::sin(-angle) + uy * hl * std::cos(-angle));
//
//                QPolygonF poly;
//                poly << b << p1 << p2;
//                p.setBrush(it.color);
//                p.drawPolygon(poly);
//                p.setBrush(Qt::NoBrush);
//            }
//            break;
//        }
//        case DrawItem::Type::kPen: {
//            if (it.path.size() >= 2) {
//                QPen pen(it.color, it.stroke_width,
//                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
//                p.setPen(pen);
//                QPainterPath path;
//                path.moveTo(it.path[0]);
//                for (int i = 1; i < it.path.size(); ++i) {
//                    path.lineTo(it.path[i]);
//                }
//                p.drawPath(path);
//            }
//            else if (it.path.size() == 1) {
//                QPen pen(it.color, it.stroke_width,
//                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
//                p.setPen(pen);
//                p.drawPoint(it.path[0]);
//            }
//            break;
//        }
//        default:
//            // 像素级的在上面已经处理过，这里忽略
//            break;
//        }
//        };
//
//    for (const DrawItem& it : items_) {
//        drawVectorItem(it);
//    }
//    if (preview) {
//        drawVectorItem(*preview);
//    }
//
//    p.end();
//    update();
//}
//
//// 简单的 hit-test：基于 bounding box 扩展橡皮半径，或检测路径点距离
//bool ScreenshotOverlay::ItemHitTest(const DrawItem& item, const QVector<QPoint>& eraserPath, int eraserRadius) const {
//    if (eraserPath.isEmpty()) return false;
//    QRect itemRect = item.rect;
//    if (item.type == DrawItem::Type::kPen) {
//        // compute bounding rect from path
//        if (!item.path.isEmpty()) {
//            int minx = item.path[0].x(), miny = item.path[0].y();
//            int maxx = minx, maxy = miny;
//            for (const QPoint& p : item.path) {
//                minx = std::min(minx, p.x()); miny = std::min(miny, p.y());
//                maxx = std::max(maxx, p.x()); maxy = std::max(maxy, p.y());
//            }
//            itemRect = QRect(QPoint(minx, miny), QPoint(maxx, maxy));
//        }
//    }
//    else if (item.type == DrawItem::Type::kArrow) {
//        itemRect = QRect(item.p1.toPoint(), item.p2.toPoint()).normalized();
//    }
//
//    QRect eraseBounds;
//    // compute bounding box of eraser path
//    int minx = eraserPath[0].x(), miny = eraserPath[0].y();
//    int maxx = minx, maxy = miny;
//    for (const QPoint& p : eraserPath) {
//        minx = std::min(minx, p.x()); miny = std::min(miny, p.y());
//        maxx = std::max(maxx, p.x()); maxy = std::max(maxy, p.y());
//    }
//    eraseBounds = QRect(QPoint(minx - eraserRadius, miny - eraserRadius),
//        QPoint(maxx + eraserRadius, maxy + eraserRadius));
//
//    if (itemRect.intersects(eraseBounds)) return true;
//
//    // 更精确：对 pen 路径点检测到擦除路径的最小距离
//    if (item.type == DrawItem::Type::kPen && !item.path.isEmpty()) {
//        for (const QPoint& p1 : item.path) {
//            for (const QPoint& p2 : eraserPath) {
//                int dx = p1.x() - p2.x();
//                int dy = p1.y() - p2.y();
//                if (dx * dx + dy * dy <= eraserRadius * eraserRadius) return true;
//            }
//        }
//    }
//
//    return false;
//}
// ---------- 合成渲染：从 base_pixmap_ + items_ 重新生成 canvas_ ----------
void ScreenshotOverlay::RepaintCanvasFromItems(const DrawItem* preview /*= nullptr*/) {
    if (base_pixmap_.isNull()) return;
    canvas_ = base_pixmap_.copy();

    auto drawItem = [&](const DrawItem& it) {
        switch (it.type) {
        case DrawItem::Type::kRect: {
            ShapeDrawer::DrawRect(canvas_, it.rect, it.color, it.stroke_width);
            break;
        }
        case DrawItem::Type::kEllipse: {
            ShapeDrawer::DrawEllipse(canvas_, it.rect, it.color, it.stroke_width);
            break;
        }
        case DrawItem::Type::kArrow: {
            ShapeDrawer::DrawArrow(canvas_, it.p1.toPoint(), it.p2.toPoint(), it.color, it.stroke_width);
            break;
        }
        case DrawItem::Type::kPen: {
            ShapeDrawer::DrawPen(canvas_, it.path, it.color, it.stroke_width);
            break;
        }
        case DrawItem::Type::kMosaic: {
            // 使用现有马赛克工具，注意传入的是 canvas_ 的坐标系（area 相对于 canvas）
            if (mosaicTool_) {
                MosaicTool::applyEffect(canvas_, it.rect, it.mosaic_level);
            }
            break;
        }
        case DrawItem::Type::kBlur: {
            if (blurTool_) {
                BlurTool::applyEffect(canvas_, it.rect, it.blur_opacity);
            }
            break;
        }
        case DrawItem::Type::kEraserFree: {
            ShapeDrawer::ErasePen(canvas_, it.path, it.stroke_width);
            break;
        }
        default:
            break;
        }
        };

    // 绘制已提交 items_
    for (const DrawItem& it : items_) {
        drawItem(it);
    }

    // 绘制 preview（不加入 items）
    if (preview) {
        drawItem(*preview);
    }

    update();
}

// 简单的 hit-test：基于 bounding box 扩展橡皮半径，或检测路径点距离
bool ScreenshotOverlay::ItemHitTest(const DrawItem& item, const QVector<QPoint>& eraserPath, int eraserRadius) const {
    if (eraserPath.isEmpty()) return false;
    QRect itemRect = item.rect;
    if (item.type == DrawItem::Type::kPen) {
        // compute bounding rect from path
        if (!item.path.isEmpty()) {
            int minx = item.path[0].x(), miny = item.path[0].y();
            int maxx = minx, maxy = miny;
            for (const QPoint& p : item.path) {
                minx = std::min(minx, p.x()); miny = std::min(miny, p.y());
                maxx = std::max(maxx, p.x()); maxy = std::max(maxy, p.y());
            }
            itemRect = QRect(QPoint(minx, miny), QPoint(maxx, maxy));
        }
    }
    else if (item.type == DrawItem::Type::kArrow) {
        itemRect = QRect(item.p1.toPoint(), item.p2.toPoint()).normalized();
    }

    QRect eraseBounds;
    // compute bounding box of eraser path
    int minx = eraserPath[0].x(), miny = eraserPath[0].y();
    int maxx = minx, maxy = miny;
    for (const QPoint& p : eraserPath) {
        minx = std::min(minx, p.x()); miny = std::min(miny, p.y());
        maxx = std::max(maxx, p.x()); maxy = std::max(maxy, p.y());
    }
    eraseBounds = QRect(QPoint(minx - eraserRadius, miny - eraserRadius),
        QPoint(maxx + eraserRadius, maxy + eraserRadius));

    if (itemRect.intersects(eraseBounds)) return true;

    // 更精确：对 pen 路径点检测到擦除路径的最小距离
    if (item.type == DrawItem::Type::kPen && !item.path.isEmpty()) {
        for (const QPoint& p1 : item.path) {
            for (const QPoint& p2 : eraserPath) {
                int dx = p1.x() - p2.x();
                int dy = p1.y() - p2.y();
                if (dx * dx + dy * dy <= eraserRadius * eraserRadius) return true;
            }
        }
    }

    return false;
}
void ScreenshotOverlay::DoHoverInspect(const QPoint& pos)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);

    QPoint globalPos = mapToGlobal(pos);

    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen) screen = QGuiApplication::primaryScreen();

    const qreal scale = screen ? screen->devicePixelRatio() : 1.0;

    POINT nativePt;
    nativePt.x = qRound(globalPos.x() * scale);
    nativePt.y = qRound(globalPos.y() * scale);

    QRect screenRect = ui_inspector_.quickInspect(nativePt, hwnd);

    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    if (screenRect.isValid()) {
        QRectF logicalRect(
            screenRect.left() / scale,
            screenRect.top() / scale,
            screenRect.width() / scale,
            screenRect.height() / scale
        );
        hover_rect_ = logicalRect.toRect().intersected(rect());
        hover_valid_ = hover_rect_.isValid();   
    }
    else {
        hover_valid_ = false;
    }

    update();
#endif
}

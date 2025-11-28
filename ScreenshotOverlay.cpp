#include "ScreenshotOverlay.h"
#include"ShapeDrawer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QColorDialog>

ScreenshotOverlay::ScreenshotOverlay(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::Tool);
    setWindowState(Qt::WindowFullScreen);

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);  // 接受 ESC 键

    // 工具栏作为子控件（不是顶层窗口），初始隐藏
    toolbar_ = new EditorToolbar(this);
    toolbar_->setFixedHeight(36);
    toolbar_->hide();

    connect(toolbar_, &EditorToolbar::ToolSelected,
        this, &ScreenshotOverlay::OnToolSelected);
}

void ScreenshotOverlay::SetBackground(const QPixmap& pixmap) {
    background_ = pixmap;
    update();
}

void ScreenshotOverlay::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);

    // 背景：原始屏幕截图
    painter.drawPixmap(0, 0, background_);

    // 全屏半透明遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 120));

    // 绘制选区内容和边框
    if (!selection_.isNull()) {
        if (stage_ == Stage::kEditing && !canvas_.isNull()) {
            // 编辑阶段：把画布画到选区位置
            painter.drawPixmap(selection_.topLeft(), canvas_);
        }
        else {
            // 选区阶段：仅显示原始内容
            if (!background_.isNull()) {
                QPixmap sub = background_.copy(selection_);
                painter.drawPixmap(selection_.topLeft(), sub);
            }
        }

        // 选区边框
        painter.setPen(QPen(Qt::blue, 2));
        painter.drawRect(selection_);

        //// 编辑阶段 + 正在绘制时，可以额外画辅助框（例如矩形预览）
        //if (stage_ == Stage::kEditing && is_drawing_) {
        //    painter.setPen(QPen(current_color_, 1, Qt::DashLine));
        //    QRect preview_rect(edit_start_pos_ + selection_.topLeft(),
        //        edit_current_pos_ + selection_.topLeft());
        //    painter.drawRect(preview_rect.normalized());
        //}
        // 编辑阶段 + 正在绘制时，画实时预览（使用 ShapeDrawer 的 Preview 函数）
        if (stage_ == Stage::kEditing && is_drawing_ && !canvas_.isNull()) {
            painter.save();
            // 将坐标系移动到选区左上，这样 preview 使用的是选区内的局部坐标
            painter.translate(selection_.topLeft());
            QRect preview_rect(edit_start_pos_, edit_current_pos_);
            switch (draw_mode_) {
            case DrawMode::kRect:
                ShapeDrawer::DrawRectPreview(painter, preview_rect, current_color_, 2);
                break;
            case DrawMode::kEllipse:
                ShapeDrawer::DrawEllipsePreview(painter, preview_rect, current_color_, 2);
                break;
            case DrawMode::kArrow:
                ShapeDrawer::DrawArrowPreview(painter, edit_start_pos_, edit_current_pos_, current_color_, 2);
                break;
            default:
                break;
            }
            painter.restore();
        }
    }
}
//鼠标左键按下事件
void ScreenshotOverlay::mousePressEvent(QMouseEvent* event) {
    const QPoint pos = event->pos();

    if (stage_ == Stage::kSelecting) {
        if (InsideSelection(pos)) {
            // 在已有选区内部 -> 开始移动
            is_moving_ = true;
            drag_offset_ = pos - selection_.topLeft();
            toolbar_->hide();
        }
        else {
            // 新建选区
            is_selecting_ = true;
            selection_ = QRect(pos, pos);
            toolbar_->hide();
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
            // 在绘制前保存快照，供 Undo 使用
            if (!canvas_.isNull()) {
                undo_stack_.push_back(canvas_.toImage());
                redo_stack_.clear();
            }

            is_drawing_ = true;
            edit_start_pos_ = local;
            edit_current_pos_ = local;
            // 仅刷新选区附近，减少重绘开销
            if (!selection_.isNull()) update(selection_.adjusted(-20, -20, 20, 20));            
            else update();
        }
    }
}
//鼠标移动事件
void ScreenshotOverlay::mouseMoveEvent(QMouseEvent* event) {
    const QPoint pos = event->pos();

    if (stage_ == Stage::kSelecting) {
        if (is_moving_) {
            QPoint new_top_left = pos - drag_offset_;
            selection_.moveTo(new_top_left);
            if (toolbar_->isVisible()) {
                UpdateToolbarPosition();
            }
        }
        else if (is_selecting_) {
            selection_.setBottomRight(pos);
        }
        update();
        return;
    }

    if (stage_ == Stage::kEditing && is_drawing_) {
        edit_current_pos_ = pos - selection_.topLeft();
        update();
    }
}
//鼠标左键释放事件
void ScreenshotOverlay::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);

    if (stage_ == Stage::kSelecting) {
        if (is_selecting_) {
            is_selecting_ = false;

            selection_ = selection_.normalized();
            if (selection_.width() < 5 || selection_.height() < 5) {
                selection_ = QRect();
                toolbar_->hide();
            }
            else {
                // 拉出一个新的有效选区：显示工具栏
                toolbar_->show();
                UpdateToolbarPosition();
            }
        }

        if (is_moving_) {
            is_moving_ = false;

            if (!selection_.isNull()) {
                // 移动结束：重新显示工具栏，并根据新的选区位置摆放
                toolbar_->show();
                UpdateToolbarPosition();
            }
        }

        update();
        return;
    }


    // 编辑阶段：结束绘制
    if (stage_ == Stage::kEditing && is_drawing_) {
        is_drawing_ = false;

        if (canvas_.isNull()) {
            return;
        }

        QPoint end_local = edit_current_pos_;//鼠标拖动过程中实时更新的点，松开那一刻就是“终点”
        // 保证点在 canvas 范围内
        auto clampPoint = [&](QPoint p) {
            p.setX(qBound(0, p.x(), canvas_.width() - 1));
            p.setY(qBound(0, p.y(), canvas_.height() - 1));
            return p;
            };
        //
        //edit_start_pos就是鼠标开始的点
        QPoint start = clampPoint(edit_start_pos_);
        QPoint end = clampPoint(end_local);
        QRect area(edit_start_pos_, end_local);
        area = area.normalized();
        switch (draw_mode_) {
        case DrawMode::kRect: {
            ShapeDrawer::DrawRect(canvas_, area, current_color_, 2, false);
            break;
        }
        case DrawMode::kEllipse: {
            ShapeDrawer::DrawEllipse(canvas_, area, current_color_, 2, false);
            break;
        }
        case DrawMode::kArrow: {
            ShapeDrawer::DrawArrow(canvas_, start, end, current_color_, 2);
            break;
        }
        case DrawMode::kMosaic:

            break;
        case DrawMode::kBlur:

            break;
        default:
            break;
        }

        modified_ = true;
        update();
    }
}

bool ScreenshotOverlay::InsideSelection(const QPoint& pos) const {
    return selection_.contains(pos);
}
//键盘事件
void ScreenshotOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
    }
}

void ScreenshotOverlay::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    if (toolbar_ && toolbar_->isVisible() && !selection_.isNull()) {
        UpdateToolbarPosition();
    }
}

// 工具栏位置：选区下方，右边缘对齐选区右边缘
void ScreenshotOverlay::UpdateToolbarPosition() {
    if (!toolbar_ || selection_.isNull()) {
        return;
    }

    const QSize tb_size = toolbar_->sizeHint();
    const int w = tb_size.width();
    const int h = toolbar_->height() > 0 ? toolbar_->height() : tb_size.height();

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

void ScreenshotOverlay::StartEditingIfNeeded() {
    if (stage_ == Stage::kEditing) {
        return;
    }
    if (selection_.isNull() || background_.isNull()) {
        return;
    }

    canvas_ = background_.copy(selection_);
    modified_ = false;
    undo_stack_.clear();
    redo_stack_.clear();

    stage_ = Stage::kEditing;
}
//得到现在的结果图像：优先返回编辑画布，否则原始选区
QPixmap ScreenshotOverlay::CurrentResultPixmap() const {
    if (stage_ == Stage::kEditing && !canvas_.isNull()) {
        return canvas_;
    }
    if (!selection_.isNull() && !background_.isNull()) {
        return background_.copy(selection_);
    }
    return QPixmap();
}
//复制到剪切板
void ScreenshotOverlay::CopyResultToClipboard() {
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }
    QClipboard* cb = QGuiApplication::clipboard();
    cb->setPixmap(result);
}
//保存到本地
void ScreenshotOverlay::SaveToFile() {
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Screenshot"),
        QString(),
        tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;Bitmap (*.bmp)"));
    if (path.isEmpty()) {
        return;
    }

    result.save(path);
}
//AI
void ScreenshotOverlay::RunAiOcr() {
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }
    // TODO: 打开一个新窗口，左侧显示 result，右侧显示识别出的文字
}
//AI 描述
void ScreenshotOverlay::RunAiDescribe() {
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }
    // TODO: 打开一个新窗口，左侧显示 result，右侧显示 AI 生成的描述
}
//pin
void ScreenshotOverlay::PinToDesktop() {
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }
    // TODO: 创建一个无边框、置顶的小窗口显示 result，相当于“贴在桌面”
}

void ScreenshotOverlay::Undo() {
    if (undo_stack_.isEmpty() || canvas_.isNull()) {
        return;
    }
    redo_stack_.push_back(canvas_.toImage());
    canvas_ = QPixmap::fromImage(undo_stack_.takeLast());
    modified_ = true;
    update();
}

void ScreenshotOverlay::Redo() {
    if (redo_stack_.isEmpty() || canvas_.isNull()) {
        return;
    }
    undo_stack_.push_back(canvas_.toImage());
    canvas_ = QPixmap::fromImage(redo_stack_.takeLast());
    modified_ = true;
    update();
}

void ScreenshotOverlay::OnToolSelected(EditorToolbar::Tool tool) {
    switch (tool) {
        // 绘图工具：固定选区，进入编辑阶段 + 切换模式
    case EditorToolbar::Tool::kRect:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kRect;
        break;
    case EditorToolbar::Tool::kEllipse:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kEllipse;
        break;
    case EditorToolbar::Tool::kArrow:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kArrow;
        break;
    case EditorToolbar::Tool::kMosaic:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kMosaic;
        break;
    case EditorToolbar::Tool::kBlur:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kBlur;
        break;

    case EditorToolbar::Tool::kColor: {
        StartEditingIfNeeded();  // 可选：也可以允许在选区阶段就换颜色
        QColor c =
            QColorDialog::getColor(current_color_, this, tr("Select Color"));
        if (c.isValid()) {
            current_color_ = c;
            // TODO: 同步到工具栏 Color 按钮的外观
        }
        break;
    }

    case EditorToolbar::Tool::kUndo:
        Undo();
        break;
    case EditorToolbar::Tool::kRedo:
        Redo();
        break;

    case EditorToolbar::Tool::kAiOcr:
        StartEditingIfNeeded();
        RunAiOcr();
        break;
    case EditorToolbar::Tool::kAiDescribe:
        StartEditingIfNeeded();
        RunAiDescribe();
        break;

    case EditorToolbar::Tool::kLongShot:
        // TODO: 实现长截图逻辑
        break;

    case EditorToolbar::Tool::kPin:
        StartEditingIfNeeded();
        PinToDesktop();
        close();
        break;

    case EditorToolbar::Tool::kSave:
        StartEditingIfNeeded();
        SaveToFile();
        break;

    case EditorToolbar::Tool::kCopy:
        StartEditingIfNeeded();
        CopyResultToClipboard();
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
#include "ScreenshotOverlay.h"
#include "OCR.h"
#include "OcrResultDialog.h"

#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QColorDialog>
#include <QWheelEvent>
#include <QTimer>
#include <QPointer>
#include "PinnedWindow.h"

class MosaicBlurController;
extern MosaicBlurController* g_mosaicBlurController;

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

    // 初始化工具
    mosaicTool_ = new MosaicTool(this);
    blurTool_ = new BlurTool(this);

    // 创建二级设置栏（初始隐藏）
    mosaicPopup_ = mosaicTool_->createSettingsWidget(this);
    mosaicPopup_->hide();

    blurPopup_ = blurTool_->createSettingsWidget(this);
    blurPopup_->hide();
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
        painter.save();
        QRect target = selection_;
        if (stage_ == Stage::kEditing && !canvas_.isNull()) {
            QRect src = ComputeZoomSourceRect(canvas_.size());
            painter.drawPixmap(target, canvas_, src);
        } else {
            if (!background_.isNull()) {
                QPixmap sub = background_.copy(selection_);
                QRect src = ComputeZoomSourceRect(sub.size());
                painter.drawPixmap(target, sub, src);
            }
        }
        painter.restore();

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
            update();
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
                toolbar_->show();
                UpdateToolbarPosition();
                zoom_scale_ = 1.0;
                zoom_center_ = QPointF(selection_.width() / 2.0, selection_.height() / 2.0);
            }
        }

        if (is_moving_) {
            is_moving_ = false;

            if (!selection_.isNull()) {
                toolbar_->show();
                UpdateToolbarPosition();
                zoom_center_ = QPointF(selection_.width() / 2.0, selection_.height() / 2.0);
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
        //edit_start_pos就是鼠标开始的点
        QRect area(edit_start_pos_, end_local);
        area = area.normalized();
        switch (draw_mode_) {
        case DrawMode::kRect: {
            //调用队友的接口
            break;
        }
        case DrawMode::kEllipse: {
            break;
        }
        case DrawMode::kArrow: {
     
            break;
        }
        case DrawMode::kMosaic: {
            if (!canvas_.isNull()) {
                MosaicTool::applyEffect(canvas_, area, mosaicTool_->blurLevel());
            }
            break;
        }
        case DrawMode::kBlur: {
            if (!canvas_.isNull()) {
                BlurTool::applyEffect(canvas_, area, blurTool_->opacity());
            }
            break;
        }
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

void ScreenshotOverlay::wheelEvent(QWheelEvent* event) {
    if (selection_.isNull()) {
        return;
    }
    const int delta = event->angleDelta().y();
    if (delta == 0) return;
    QPoint p = event->position().toPoint();
    if (selection_.contains(p)) {
        QPoint local = p - selection_.topLeft();
        zoom_center_ = QPointF(local);
    }
    double factor = (delta > 0) ? 1.1 : 0.9;
    zoom_scale_ *= factor;
    if (zoom_scale_ < 0.2) zoom_scale_ = 0.2;
    if (zoom_scale_ > 8.0) zoom_scale_ = 8.0;
    update();
}

QRect ScreenshotOverlay::ComputeZoomSourceRect(const QSize& content_size) const {
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
    
    // 直接复用 OcrEngine 和 OcrResultDialog
    auto* engine = &OcrEngine::instance();
    
    // 创建并显示结果对话框
    auto* dlg = new OcrResultDialog(result, "Processing...", nullptr);
    // 居中显示
    dlg->move(this->geometry().center() - dlg->rect().center());
    dlg->show();
    
    // 使用 QPointer
    QPointer<OcrResultDialog> safeDlg(dlg);

    QTimer::singleShot(100, [result, safeDlg, engine]() {
        if (!safeDlg) return;
        
        QString text;
        try {
            text = engine->detectText(result.toImage());
        } catch (const std::exception& e) {
            text = QString("Error: %1").arg(e.what());
        } catch (...) {
            text = "Unknown Error";
        }
        
        if (safeDlg) {
            safeDlg->SetResultText(text);
        }
    });
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
    auto* pin = PinnedWindow::CreatePinnedWindow(result, nullptr);
    if (pin) {
        pin->setOcrEnabled(true);
    }
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
        // 显示马赛克设置栏
        showToolPopup(mosaicPopup_);
        break;
    case EditorToolbar::Tool::kBlur:
        StartEditingIfNeeded();
        draw_mode_ = DrawMode::kBlur;
        // 显示模糊设置栏
        showToolPopup(blurPopup_);
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
    case EditorToolbar::Tool::kOcr: {
        StartEditingIfNeeded();
        
        QPixmap result = CurrentResultPixmap();
        if (result.isNull()) break;

        // 1. 创建并显示结果对话框 (Parent设为nullptr以独立于Overlay)
        auto* dlg = new OcrResultDialog(result, "Recognizing...", nullptr);
        dlg->move(this->geometry().center() - dlg->rect().center());
        dlg->show();

        // 2. 关闭截图区域
        close();

        // 3. 异步执行 OCR，避免阻塞 UI 关闭
        // 使用 QPointer 确保安全
        QPointer<OcrResultDialog> safeDlg(dlg);
        
        QTimer::singleShot(100, [result, safeDlg]() {
            if (!safeDlg) return;

            auto* engine = &OcrEngine::instance();
            QString text;
            try {
                // 同步调用
                text = engine->detectText(result.toImage());
            } catch (const std::exception& e) {
                text = QString("Error: %1").arg(e.what());
            } catch (...) {
                text = "Unknown Error during OCR dispatch.";
            }
            
            if (safeDlg) {
                safeDlg->SetResultText(text);
            }
        });
        break;
    }

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

void ScreenshotOverlay::showToolPopup(QWidget* popup) {
    if (!popup || !toolbar_) return;

    // 获取工具栏在主屏幕的位置
    QPoint toolbarPos = toolbar_->mapToGlobal(QPoint(0, 0));
    QRect toolbarRect = toolbar_->rect();

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

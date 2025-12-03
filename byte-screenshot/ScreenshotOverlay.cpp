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
#include "PinnedWindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
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

    // 放大镜：使用整屏截图作为源
    magnifier_.setSourcePixmap(&background_);
    magnifier_.setLensSize(120);  // 可调
    magnifier_.setZoomFactor(4);  // 可调
    magnifier_.setEnabled(true);
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
        painter.setPen(QPen(QColor(0, 120, 255), 2));
        painter.setBrush(QColor(0, 120, 255, 50));
        painter.drawRect(hover_rect_);
        painter.restore();
    }
#endif

    // 绘制选区内容和边框
    if (!selection_.isNull()) {
        painter.save();
        QRect target = selection_;
        if (stage_ == Stage::kEditing && !canvas_.isNull()) {
            QRect src = ComputeZoomSourceRect(canvas_.size());
            painter.drawPixmap(target, canvas_, src);
        }
        else {
            if (!background_.isNull()) {
                QPixmap sub = background_.copy(selection_);
                QRect   src = ComputeZoomSourceRect(sub.size());
                painter.drawPixmap(target, sub, src);
            }
        }
        painter.restore();

        // 选区边框
        painter.setPen(QPen(Qt::blue, 2));
        painter.drawRect(selection_);
    }

    // 最后统一绘制放大镜（基于当前鼠标位置）
    magnifier_.paint(painter, rect());
}

// 鼠标左键按下事件
void ScreenshotOverlay::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos = event->pos();

    if (stage_ == Stage::kSelecting) {
        if (InsideSelection(pos)) {
            // 在已有选区内部 -> 开始移动
            is_moving_ = true;
            drag_offset_ = pos - selection_.topLeft();
            toolbar_->hide();
            if (sToolbar_) sToolbar_->hide();
        }
        else {
            // 新建选区
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

// 鼠标移动事件
void ScreenshotOverlay::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->pos();
    cursor_pos_ = pos;                 // 放大镜用
    magnifier_.setCursorPos(pos);

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
        }
        else {
            // 没在拉选区、也没在拖动：做自动窗口识别（Hover 高亮）
#ifdef Q_OS_WIN
            // 1. 暂时允许鼠标穿透当前 overlay，避免 UIAutomation 命中自己
            HWND hwnd = reinterpret_cast<HWND>(winId());
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);

            // 2. 使用 UIAutomation 通过全局坐标获取控件/窗口矩形（屏幕坐标）
            QPoint globalPos = mapToGlobal(pos);
            QRect  screenRect = ui_inspector_.quickInspect(globalPos);

            // 3. 恢复窗口样式
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

            if (screenRect.isValid()) {
                // 转成当前 overlay 内部坐标
                QRect widgetRect(mapFromGlobal(screenRect.topLeft()),
                    screenRect.size());
                hover_rect_ = widgetRect.intersected(rect());
                hover_valid_ = hover_rect_.isValid();
            }
            else {
                hover_valid_ = false;
            }
#endif
        }
        update();
        return;
    }

    if (stage_ == Stage::kEditing && is_drawing_) {
        edit_current_pos_ = pos - selection_.topLeft();
        update();
        return;
    }

    // 其它阶段也重绘一下（放大镜位置会变）
    update();
}

// 鼠标左键释放事件
void ScreenshotOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);

    if (stage_ == Stage::kSelecting) {
        if (is_selecting_) {
            is_selecting_ = false;

            selection_ = selection_.normalized();
            if (selection_.width() < 5 || selection_.height() < 5) {
                selection_ = QRect();
                toolbar_->hide();
                if (sToolbar_) sToolbar_->hide();
            }
            else {
                toolbar_->show();
                UpdateToolbarPosition();
                UpdateSecondaryToolbarPosition();
                zoom_scale_ = 1.0;
                zoom_center_ = QPointF(selection_.width() / 2.0,
                    selection_.height() / 2.0);
            }
        }

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

        update();
        return;
    }

    // 编辑阶段：结束绘制（Rect / Ellipse / Arrow / Mosaic / Blur）
    if (stage_ == Stage::kEditing && is_drawing_) {
        is_drawing_ = false;

        if (canvas_.isNull()) {
            return;
        }

        QPoint end_local = edit_current_pos_;// 鼠标拖动终点
        QRect  area(edit_start_pos_, end_local);
        area = area.normalized();

     

        switch (draw_mode_) {
        case DrawMode::kRect: {
            //TODO    矩形
            break;
        }
        case DrawMode::kEllipse: {
            // 椭圆
            break;
        }
        case DrawMode::kArrow: {
            // TODO: 箭头绘制算法（这里先留空）
            break;
        }
        case DrawMode::kPen:
        case DrawMode::kEraser:
            break;
        case DrawMode::kMosaic: {
            MosaicTool::applyEffect(canvas_, area, mosaicTool_->blurLevel());
            if (mosaicPopup_) {
                showToolPopup(mosaicPopup_);
            }
            break;
        }
        case DrawMode::kBlur: {
            BlurTool::applyEffect(canvas_, area, blurTool_->opacity());
            if (blurPopup_) {
                showToolPopup(blurPopup_);
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

bool ScreenshotOverlay::InsideSelection(const QPoint& pos) const
{
    return selection_.contains(pos);
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

    canvas_ = background_.copy(selection_);
    modified_ = false;
    undo_stack_.clear();
    redo_stack_.clear();

    stage_ = Stage::kEditing;
}

//得到现在的结果图像：优先返回编辑画布，否则原始选区
QPixmap ScreenshotOverlay::CurrentResultPixmap() const
{
    if (stage_ == Stage::kEditing && !canvas_.isNull()) {
        return canvas_;
    }
    if (!selection_.isNull() && !background_.isNull()) {
        return background_.copy(selection_);
    }
    return QPixmap();
}

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

//OCR
void ScreenshotOverlay::RunAiOcr()
{
    QPixmap result = CurrentResultPixmap();
    if (result.isNull()) {
        return;
    }
    // TODO: 打开一个新窗口，左侧显示 result，右侧显示识别出的文字
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
    PinnedWindow::Pinned(result, nullptr);
}

void ScreenshotOverlay::Undo()
{
    if (undo_stack_.isEmpty() || canvas_.isNull()) {
        return;
    }
    redo_stack_.push_back(canvas_.toImage());
    canvas_ = QPixmap::fromImage(undo_stack_.takeLast());
    modified_ = true;
    update();
}

void ScreenshotOverlay::Redo()
{
    if (redo_stack_.isEmpty() || canvas_.isNull()) {
        return;
    }
    undo_stack_.push_back(canvas_.toImage());
    canvas_ = QPixmap::fromImage(redo_stack_.takeLast());
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

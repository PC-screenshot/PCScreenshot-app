#include "EditorToolbar.h"

#include <QToolButton>
#include <QHBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QVariant>

namespace {

    // 配置每个工具：类型、显示文字、是否可选中、分组 id、未来图标路径
    struct ToolConfig {
        EditorToolbar::Tool tool;
        const char* text;
        bool checkable;
        int group;              // 用于插入分隔线
        const char* icon_path;  // 未来可以用 QIcon 替代文字
    };

    using Tool = EditorToolbar::Tool;

    // 这里定义工具栏按钮顺序和分组
    // group: 0 = 绘图相关, 1 = AI / 长截图 / Pin, 2 = 保存/取消/完成
    const ToolConfig kToolConfigs[] = {
        // —— 绘制类 —— 
     {Tool::kRect,       "Rect",     true,  0, ":/icons/icons8-rectangle-40.png"},   // 矩形
     {Tool::kEllipse,    "Ellipse",  true,  0, ":/icons/icons8-ellipse-40.png"},     // 椭圆
     {Tool::kArrow,      "Arrow",    true,  0, ":/icons/icons8-arrow-40.png"},       // 箭头
     {Tool::kPen,        "Pen",      true,  0, ":/icons/icons8-edit-pencil-50.png"}, // 画笔
     {Tool::kEraser,     "Eraser",   true,  0, ":/icons/icons8-eraser-40.png"},      // 橡皮擦

     {Tool::kMosaic,     "Mosaic",   true,  0, ":/icons/icons8-modern-art-48.png"},  // 马赛克
     {Tool::kBlur,       "Blur",     true,  0, ":/icons/icons8-blur-50.png"},        // 模糊

     {Tool::kUndo,       "Undo",     false, 0, ":/icons/icons8-undo-40.png"},        // 撤销
     {Tool::kRedo,       "Redo",     false, 0, ":/icons/icons8-redo-40.png"},        // 重做

     // —— AI / 滚动 / Pin —— 
     {Tool::kOcr,        "OCR",      false, 1, ":/icons/icons8-ocr-40.png"},         // 文字识别
     {Tool::kAiDescribe, "AI-Desc",  false, 1, ":/icons/icons8-ai-50.png"},          // AI 描述
     {Tool::kLongShot,   "Scroll",   false, 1, ":/icons/icons8-double-down-50.png"}, // 长截图
     {Tool::kPin,        "Pin",      false, 1, ":/icons/icons8-pin-50.png"},         // Pin 到桌面

     // —— 保存 / 取消 / 完成 —— 
     {Tool::kSave,       "Save",     false, 2, ":/icons/icons8-save-40.png"},        // 保存到文件
     {Tool::kCancel,     "Cancel",   false, 2, ":/icons/icons8-multiply-40.png"},    // 取消
     {Tool::kDone,       "Done",     false, 2, ":/icons/icons8-done-40.png"},        // 完成
    };

    // 可选中工具的样式（Rect / Arrow / Pen / Eraser / Mosaic / Blur 等）
    const char* kSelectableButtonStyle = R"(
  QToolButton {
    color: #444444;
    background: transparent;
    border-radius: 4px;
    padding: 2px 2px;
  }
  QToolButton:hover {
    background: rgba(0, 0, 0, 0.08);
  }
  QToolButton:checked {
    background: rgba(0, 0, 0, 0.18);
    border: 1px solid #666666;
  }
)";

    // 一次性动作按钮的样式（Undo / Save / AI / Pin 等）
    const char* kActionButtonStyle = R"(
  QToolButton {
    color: #555555;
    background: transparent;
    border-radius: 4px;
    padding: 2px 2px;
  }
  QToolButton:hover {
    background: rgba(0, 0, 0, 0.06);
  }
)";

}  // namespace

EditorToolbar::EditorToolbar(QWidget* parent)
    : QWidget(parent) {
    // 作为浮动工具条使用（父级一般是 ScreenshotOverlay）
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedHeight(40);

    InitUi();
}

void EditorToolbar::InitUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setSpacing(2);
    layout->setContentsMargins(6, 4, 6, 4);

    int last_group = -1;

    for (const ToolConfig& cfg : kToolConfigs) {
        if (last_group != -1 && cfg.group != last_group) {
            // 分组变化时插入一条竖直分隔线
            auto* separator = new QFrame(this);
            separator->setFrameShape(QFrame::VLine);
            separator->setFrameShadow(QFrame::Sunken);
            layout->addWidget(separator);
        }
        last_group = cfg.group;

        QToolButton* btn =
            CreateButton(QString::fromUtf8(cfg.text),
                cfg.tool,
                cfg.checkable,
                QString::fromUtf8(cfg.icon_path));

        layout->addWidget(btn);
        buttons_.push_back(btn);
    }
}
QToolButton* EditorToolbar::CreateButton(const QString& text,
    Tool tool,
    bool checkable,
    const QString& icon_path)
{
    auto* button = new QToolButton(this);

    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFixedSize(32, 32);
    button->setIconSize(QSize(24, 24));
    button->setToolTip(text);

    button->setProperty("tool", static_cast<int>(tool));

    if (checkable) {
        button->setCheckable(true);
        if (tool == current_tool_) {
            button->setChecked(true);
        }
        button->setStyleSheet(QString::fromUtf8(kSelectableButtonStyle));
    } else {
        button->setStyleSheet(QString::fromUtf8(kActionButtonStyle));
    }

    // === 关键检查逻辑 ===
    if (!icon_path.isEmpty()) {
        QIcon icon(icon_path);
        if (!icon.isNull()) {
            button->setIcon(icon);
        } else {
            // 图标没加载成功：显示文字 + 红色背景，方便你肉眼检查
            button->setText(text.left(2)); // 显示前两个字母
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
            button->setStyleSheet(
                "QToolButton { background: rgba(255,0,0,0.2); border-radius:4px; }");
        }
    } else {
        button->setText(text.left(2));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    connect(button, &QToolButton::clicked, this,
        [this, tool, checkable]() {
            if (checkable) {
                SetCurrentTool(tool);
            }
            emit ToolSelected(tool);
        });

    return button;
}

void EditorToolbar::SetCurrentTool(Tool tool) {
    if (current_tool_ == tool) {
        return;
    }
    current_tool_ = tool;

    // 更新所有可选中按钮的 checked 状态
    for (QToolButton* btn : buttons_) {
        if (!btn) continue;
        QVariant v = btn->property("tool");
        if (!v.isValid()) continue;

        Tool btn_tool = static_cast<Tool>(v.toInt());
        if (!btn->isCheckable()) {
            continue;
        }
        btn->setChecked(btn_tool == current_tool_);
    }
}

void EditorToolbar::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景：浅灰色圆角矩形
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(245, 245, 245, 240));
    painter.drawRoundedRect(rect(), 6, 6);

    // 边框
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
}

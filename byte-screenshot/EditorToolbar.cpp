#include "EditorToolbar.h"

#include <QToolButton>
#include <QHBoxLayout>
#include <QFrame>
#include <QPainter>

namespace {

    // 配置每个工具：类型、显示文字、是否可选中、分组 id、未来图标路径
    struct ToolConfig {
        EditorToolbar::Tool tool;
        const char* text;
        bool checkable;
        int group;                // 用于插入分隔线
        const char* icon_path;    // 未来可以用 QIcon 替代文字
    };

    // 为了简写
    using Tool = EditorToolbar::Tool;

    // 这里定义工具栏按钮顺序和分组
    const ToolConfig kToolConfigs[] = {
        {Tool::kColor,      "Color",  false, 0, ""},//颜色按钮，矩形框、椭圆框、箭头的颜色
        {Tool::kRect,       "Rect",   true,  0, ""},//矩形框
        {Tool::kEllipse,    "Ellipse",true,  0, ""},//椭圆框
        {Tool::kArrow,      "Arrow",  true,  0, ""},//箭头
		{Tool::kMosaic,     "Mosaic", true,  0, ""},//马赛克，也能选择强度
        {Tool::kBlur,       "Blur",   true,  0, ""},//高斯模糊，能选择强度
		{Tool::kUndo,       "Undo",   false, 0, ""},//撤销
		{Tool::kRedo,       "Redo",   false, 0, ""},//重做

		{Tool::kAiOcr,      "AI-OCR", false, 1, ""},//AI文字识别
		{Tool::kAiDescribe, "AI-Desc",false, 1, ""},//AI描述
        {Tool::kLongShot,   "Scroll", false, 1, ""},//长截图
		{Tool::kPin,        "Pin",    false, 1, ""},//固定截图窗口
        {Tool::kOcr,        "OCR",    false, 1, ""},//OCR识别

		{Tool::kSave,       "Save",   false, 2, ""},//保存到文件
		{Tool::kCancel,     "Cancel", false, 2, ""},//取消截图和编辑
		{Tool::kDone,       "Done",   false, 2, ""},//完成截图和编辑，复制到剪切板

    };

    // 可选中工具的样式（Rect / Arrow / Pen 等）
    const char* kSelectableButtonStyle = R"(
  QToolButton {
    color: #444444;
    background: transparent;
    border-radius: 4px;
    padding: 4px 6px;
  }
  QToolButton:hover {
    background: rgba(0, 0, 0, 0.08);
  }
  QToolButton:checked {
    background: rgba(0, 0, 0, 0.18);
    border: 1px solid #666666;
  }
)";

    // 一次性动作按钮的样式（Undo / Save / Copy / AI 等）
    const char* kActionButtonStyle = R"(
  QToolButton {
    color: #555555;
    background: transparent;
    border-radius: 4px;
    padding: 4px 6px;
  }
  QToolButton:hover {
    background: rgba(0, 0, 0, 0.06);
  }
)";

}  // namespace

EditorToolbar::EditorToolbar(QWidget* parent)
    : QWidget(parent) {
    //// 做成浮窗：无边框、置顶、小工具窗口
    //setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setFixedHeight(36);

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
            CreateButton(QString::fromUtf8(cfg.text), cfg.tool, cfg.checkable,
                QString::fromUtf8(cfg.icon_path));

        layout->addWidget(btn);
        buttons_.push_back(btn);
    }
}

QToolButton* EditorToolbar::CreateButton(const QString& text,
    Tool tool,
    bool checkable,
    const QString& icon_path) {
    auto* button = new QToolButton(this);

    // 先用纯英文文字，后面根据 icon_path 换成 QIcon
    button->setText(text);
    button->setToolTip(text);
    button->setFixedSize(60, 28);
    button->setAutoRaise(true);

    button->setProperty("tool", static_cast<int>(tool));

    if (checkable) {
        button->setCheckable(true);
        // 初始选中 Move 工具
        if (tool == current_tool_) {
            button->setChecked(true);
        }
        button->setStyleSheet(QString::fromUtf8(kSelectableButtonStyle));
    }
    else {
        button->setStyleSheet(QString::fromUtf8(kActionButtonStyle));
    }

    // 之后要用图片，可以在这里判断 icon_path 是否为空：
    // if (!icon_path.isEmpty()) {
    //   button->setIcon(QIcon(icon_path));
    //   button->setText(QString());  // 不再显示文字
    // }

    // 点击逻辑：如果是模式工具 -> 更新当前工具；所有工具 -> 发信号
    connect(button, &QToolButton::clicked, this, [this, tool, checkable]() {
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


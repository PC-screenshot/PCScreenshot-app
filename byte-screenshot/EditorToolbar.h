#pragma once

#include <QWidget>
#include <QVector>
class QToolButton;
class SecondaryToolBar;
class EditorToolbar : public QWidget {
  Q_OBJECT

 public:
  enum class Tool {
    kMove,
    kColor,       // 颜色选择按钮（后期弹 QColorDialog）
    kRect,
    kEllipse,
    kLine,
    kArrow,
    kPen,
    kHighlighter,
    kMosaic,
    kBlur,
    kText,
    kEraser,
    kAiOcr,
    kAiDescribe,
    kPin,
    kLongShot,
    kUndo,
    kRedo,
    kSave,
    kCancel,
    kDone,
  };

  explicit EditorToolbar(QWidget* parent = nullptr);

  Tool current_tool() const { return current_tool_; }
  void SetCurrentTool(Tool tool);

 signals:
  // 外部只关心“哪个工具被触发”，具体行为由上层决定
  void ToolSelected(EditorToolbar::Tool tool);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void InitUi();
  QToolButton* CreateButton(const QString& text,
                            Tool tool,
                            bool checkable,
                            const QString& icon_path = QString());

  Tool current_tool_ = Tool::kMove;
  QVector<QToolButton*> buttons_;
};

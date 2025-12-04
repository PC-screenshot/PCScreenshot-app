#pragma once

#include <QDialog>
#include <QPixmap>
#include <QPoint>
#include <QScrollArea>

class QLabel;
class QTextEdit;
class QPushButton;
class QToolButton;

class OcrResultDialog : public QDialog {
    Q_OBJECT

public:
    // 构造函数传入图片和（可选的）识别文本
    explicit OcrResultDialog(const QPixmap& pixmap, const QString& text = QString(), QWidget* parent = nullptr);

    // 设置识别结果文本
    void SetResultText(const QString& text);

    // 追加识别结果文本
    void AppendResultText(const QString& text);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void OnCopy();
    void CheckHideUi();

private:
    void SetupUi();
    void UpdateToolbarPosition();
    void UpdateImageScale();

    QPixmap pixmap_;
    QString text_;

    QLabel* image_label_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QTextEdit* text_edit_ = nullptr;
    
    // UI Components
    QPushButton* close_btn_ = nullptr; // Top-right close button
    QWidget* tool_bar_ = nullptr;      // Floating toolbar
    QToolButton* btn_copy_ = nullptr;
    
    // Dragging state
    bool dragging_ = false;
    QPoint drag_offset_;
    
    // Zoom state
    double scale_factor_ = 1.0;
};

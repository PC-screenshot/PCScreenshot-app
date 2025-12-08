#pragma once

#include <QDialog>
#include <QPixmap>
#include <QScrollArea>

class QLabel;
class QTextEdit;
class QPushButton;

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
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void OnCopy();

private:
    void SetupUi();
    void UpdateImageScale();

    QPixmap pixmap_;
    QString text_;

    QLabel* image_label_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QTextEdit* text_edit_ = nullptr;
    
    // UI Components
    QPushButton* btn_copy_ = nullptr;
    QPushButton* btn_close_ = nullptr;

    // Zoom state
    double scale_factor_ = 1.0;
};

#pragma once

#include <QDialog>
#include <QPixmap>
#include <QLineEdit>
class QLabel;
class QTextEdit;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class AiDescribeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AiDescribeDialog(const QPixmap& pixmap, QWidget* parent = nullptr);

    void setApiKey(const QString& apiKey);
    void setModel(const QString& model);

protected:
    void resizeEvent(QResizeEvent* event) override;   // 用于自适应更新图片

private slots:
    void onRequestFinished(QNetworkReply* reply);
    void onCopyTextClicked();
    void onGenerateClicked();

private:
    void initUi();                                    // 不再传 pixmap
    void updateImageDisplay();                       // 根据 label 大小 & 原图自适应
    QString pixmapToDataUrl(const QPixmap& pixmap) const;
    void sendRequest(const QPixmap& pixmap);

    // ---- data ----
    QPixmap originalPixmap_;                         // 保存原始截图

    QLabel* imageLabel_ = nullptr;
    QTextEdit* textEdit_ = nullptr;
    QPushButton* copyBtn_ = nullptr;
    QPushButton* closeBtn_ = nullptr;

    QLineEdit* promptEdit_ = nullptr;
    QPushButton* generateBtn_ = nullptr;
    QString      defaultPrompt_;

    QNetworkAccessManager* network_ = nullptr;
    QString apiKey_;
    QString model_;
};

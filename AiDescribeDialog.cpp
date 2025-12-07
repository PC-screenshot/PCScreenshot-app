//#include "AiDescribeDialog.h"
//
//#include <QLabel>
//#include <QTextEdit>
//#include <QPushButton>
//#include <QHBoxLayout>
//#include <QVBoxLayout>
//#include <QNetworkAccessManager>
//#include <QNetworkRequest>
//#include <QNetworkReply>
//#include <QJsonDocument>
//#include <QJsonObject>
//#include <QJsonArray>
//#include <QBuffer>
//#include <QGuiApplication>
//#include <QClipboard>
//
//// ================== Constructor & UI ==================
//
//AiDescribeDialog::AiDescribeDialog(const QPixmap& pixmap,
//    QWidget* parent)
//    : QDialog(parent)
//    , originalPixmap_(pixmap)
//{
//    apiKey_ = qEnvironmentVariable("ARK_API_KEY");
//    if (apiKey_.isEmpty()) {
//        apiKey_ = QStringLiteral("284143f6-2e1b-42a1-8acb-82007ebe0c1d");
//    }
//
//    defaultPrompt_ = QStringLiteral(
//        "请务必使用中文,描述这张图片的主要内容，用3到5句话描述。"
//    );
//    model_ = QStringLiteral("doubao-seed-1-6-flash-250828");
//
//    setWindowTitle(QStringLiteral("AI Describe"));
//
//    // 对话框更大一点，并限制最小尺寸
//    resize(1100, 720);
//    setMinimumSize(1000, 680);
//
//    initUi();
//
//    network_ = new QNetworkAccessManager(this);
//    connect(network_, &QNetworkAccessManager::finished,
//        this, &AiDescribeDialog::onRequestFinished);
//
//    // 首次自动用默认 prompt 生成一版描述
//    sendRequest(originalPixmap_);
//}
//
//void AiDescribeDialog::setApiKey(const QString& apiKey)
//{
//    apiKey_ = apiKey;
//}
//
//void AiDescribeDialog::setModel(const QString& model)
//{
//    model_ = model;
//}
//void AiDescribeDialog::initUi()
//{
//    auto* mainLayout = new QVBoxLayout(this);
//    mainLayout->setContentsMargins(8, 8, 8, 8);
//    mainLayout->setSpacing(8);
//
//    // ===== 中间：左图右文 =====
//    auto* centerLayout = new QHBoxLayout();
//    centerLayout->setSpacing(8);
//
//    // 左边图片
//    imageLabel_ = new QLabel(this);
//    imageLabel_->setAlignment(Qt::AlignCenter);
//    imageLabel_->setMinimumSize(520, 420);
//    imageLabel_->setSizePolicy(QSizePolicy::Expanding,
//        QSizePolicy::Expanding);
//
//    // 右边文本
//    textEdit_ = new QTextEdit(this);
//    textEdit_->setReadOnly(false);
//    textEdit_->setPlaceholderText(
//        QStringLiteral("正在向 AI 请求，请稍候……"));
//    textEdit_->setSizePolicy(QSizePolicy::Expanding,
//        QSizePolicy::Expanding);
//
//    centerLayout->addWidget(imageLabel_, 1);
//    centerLayout->addWidget(textEdit_, 1);
//
//    mainLayout->addLayout(centerLayout, /*stretch*/ 1);
//
//    // 可选：中间加一条细线做分隔
//    auto* line = new QFrame(this);
//    line->setFrameShape(QFrame::HLine);
//    line->setFrameShadow(QFrame::Sunken);
//    mainLayout->addWidget(line);
//
//    // ===== 底部一行：Prompt + Generate =====
//    auto* promptLayout = new QHBoxLayout();
//    promptLayout->setSpacing(6);
//
//    auto* promptLabel = new QLabel(QStringLiteral("Prompt:"), this);
//
//    promptEdit_ = new QLineEdit(this);
//    // 初始留空，首次请求用 defaultPrompt_
//    promptEdit_->setPlaceholderText(QStringLiteral("Enter your question or instruction here…"));
//
//    generateBtn_ = new QPushButton(QStringLiteral("Generate"), this);
//
//    promptLayout->addWidget(promptLabel);
//    promptLayout->addWidget(promptEdit_, 1);
//    promptLayout->addWidget(generateBtn_);
//
//    mainLayout->addLayout(promptLayout);
//
//    // Prompt 为空时禁用 Generate
//    generateBtn_->setEnabled(false);
//    connect(promptEdit_, &QLineEdit::textChanged,
//        this, [this](const QString& text) {
//            const bool hasText = !text.trimmed().isEmpty();
//            generateBtn_->setEnabled(hasText);
//        });
//
//    // ===== 最底部：Copy / Close 按钮行 =====
//    auto* btnLayout = new QHBoxLayout();
//    btnLayout->setSpacing(8);
//    btnLayout->addStretch();
//
//    copyBtn_ = new QPushButton(QStringLiteral("Copy"), this);
//    closeBtn_ = new QPushButton(QStringLiteral("Close"), this);
//
//    btnLayout->addWidget(copyBtn_);
//    btnLayout->addWidget(closeBtn_);
//
//    mainLayout->addLayout(btnLayout);
//
//    // ===== 信号连接 =====
//    connect(copyBtn_, &QPushButton::clicked,
//        this, &AiDescribeDialog::onCopyTextClicked);
//    connect(closeBtn_, &QPushButton::clicked,
//        this, &AiDescribeDialog::close);
//    connect(generateBtn_, &QPushButton::clicked,
//        this, &AiDescribeDialog::onGenerateClicked);
//
//    // ===== 简单深色主题美化 =====
//    setStyleSheet(
//        "QDialog {"
//        "  background-color: #2b2b2b;"
//        "  color: #f0f0f0;"
//        "}"
//        "QLabel {"
//        "  color: #f0f0f0;"
//        "}"
//        "QLineEdit, QTextEdit {"
//        "  background-color: #3c3f41;"
//        "  color: #f0f0f0;"
//        "  border: 1px solid #555555;"
//        "  selection-background-color: #5c9ded;"
//        "}"
//        "QFrame[frameShape=\"4\"] {"   // QFrame::HLine
//        "  color: #555555;"
//        "  background-color: #555555;"
//        "}"
//        "QPushButton {"
//        "  background-color: #3c3f41;"
//        "  color: #f0f0f0;"
//        "  border-radius: 4px;"
//        "  padding: 4px 14px;"
//        "}"
//        "QPushButton:hover {"
//        "  background-color: #4b4f52;"
//        "}"
//        "QPushButton:pressed {"
//        "  background-color: #2f3336;"
//        "}"
//        "QPushButton:disabled {"
//        "  background-color: #2b2d2f;"
//        "  color: #777777;"
//        "}"
//    );
//
//    // 根据当前控件尺寸渲染一次图片
//    updateImageDisplay();
//}
//
//// 根据窗口 / 左侧 label 的当前大小，等比缩放图片：
//// - 不放大，只在原图太大的时候缩小
//void AiDescribeDialog::updateImageDisplay()
//{
//    if (!imageLabel_ || originalPixmap_.isNull())
//        return;
//
//    QSize labelSize = imageLabel_->size();
//    QSize originalSize = originalPixmap_.size();
//
//    if (labelSize.isEmpty())
//        return;
//
//    // 目标尺寸：不超过 label，也不超过原图（避免被放大）
//    QSize targetSize(
//        qMin(labelSize.width(), originalSize.width()),
//        qMin(labelSize.height(), originalSize.height())
//    );
//
//    QPixmap scaled = originalPixmap_.scaled(
//        targetSize,
//        Qt::KeepAspectRatio,
//        Qt::SmoothTransformation);
//
//    imageLabel_->setPixmap(scaled);
//}
//
//// 在用户调整对话框大小时，重新计算图片显示尺寸
//void AiDescribeDialog::resizeEvent(QResizeEvent* event)
//{
//    QDialog::resizeEvent(event);
//    updateImageDisplay();
//}
//
//// ================== Pixmap -> Data URL ==================
//
//QString AiDescribeDialog::pixmapToDataUrl(const QPixmap& pixmap) const
//{
//    // Encode QPixmap as PNG -> base64 -> data:image/png;base64,xxxx
//    QBuffer buffer;
//    buffer.open(QIODevice::WriteOnly);
//    pixmap.save(&buffer, "PNG");
//    QByteArray pngData = buffer.data();
//    QByteArray base64 = pngData.toBase64();
//
//    QString dataUrl = QStringLiteral("data:image/png;base64,");
//    dataUrl += QString::fromLatin1(base64);
//    return dataUrl;
//}
//
//// ================== Send HTTP request ==================
//
//void AiDescribeDialog::sendRequest(const QPixmap& pixmap)
//{
//    if (generateBtn_) {
//        generateBtn_->setEnabled(false);
//    }
//
//    if (apiKey_.isEmpty()) {
//        textEdit_->setPlainText(
//            QStringLiteral("Please configure ARK_API_KEY (environment variable or in code) first."));
//        return;
//    }
//
//    // 1. URL & request
//    QUrl url(QStringLiteral("https://ark.cn-beijing.volces.com/api/v3/chat/completions"));
//    QNetworkRequest req(url);
//    req.setHeader(QNetworkRequest::ContentTypeHeader,
//        QStringLiteral("application/json"));
//    req.setRawHeader("Authorization",
//        QByteArray("Bearer ") + apiKey_.toUtf8());
//
//    // 2. JSON body (based on your curl example)
//    QJsonObject imageUrlObj;
//    imageUrlObj["url"] = pixmapToDataUrl(pixmap);
//
//    QJsonObject imageContent;
//    imageContent["type"] = "image_url";
//    imageContent["image_url"] = imageUrlObj;
//
//    QJsonObject textContent;
//    textContent["type"] = "text";
//
//    // 取当前 prompt 文本；为空就用默认 prompt
//    QString prompt;
//    if (promptEdit_) {
//        prompt = promptEdit_->text().trimmed();
//    }
//    if (prompt.isEmpty()) {
//        prompt = defaultPrompt_;
//    }
//    textContent["text"] = prompt;
//
//    QJsonArray contentArray;
//    contentArray.append(imageContent);
//    contentArray.append(textContent);
//
//    QJsonObject messageObj;
//    messageObj["role"] = "user";
//    messageObj["content"] = contentArray;
//
//    QJsonArray messages;
//    messages.append(messageObj);
//
//    QJsonObject root;
//    root["model"] = model_;
//    root["messages"] = messages;
//
//    QJsonDocument doc(root);
//    QByteArray body = doc.toJson(QJsonDocument::Compact);
//
//    // 3. POST
//    textEdit_->setPlainText(QStringLiteral("Requesting description from AI, please wait...\n"));
//    network_->post(req, body);
//}
//
//// ================== Handle response ==================
//
//void AiDescribeDialog::onRequestFinished(QNetworkReply* reply)
//{
//    reply->deleteLater();
//
//    if (reply->error() != QNetworkReply::NoError) {
//        textEdit_->append(
//            QStringLiteral("\nRequest failed: %1").arg(reply->errorString()));
//        return;
//    }
//
//    QByteArray data = reply->readAll();
//    QJsonParseError parseErr;
//    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
//    if (parseErr.error != QJsonParseError::NoError) {
//        textEdit_->append(
//            QStringLiteral("\nFailed to parse JSON response: %1")
//            .arg(parseErr.errorString()));
//        return;
//    }
//
//    if (!doc.isObject()) {
//        textEdit_->append(
//            QStringLiteral("\nResponse is not a JSON object."));
//        return;
//    }
//
//    QJsonObject root = doc.object();
//    QJsonArray  choices = root.value("choices").toArray();
//    if (choices.isEmpty()) {
//        textEdit_->append(
//            QStringLiteral("\nNo 'choices' field found in response."));
//        return;
//    }
//
//    QJsonObject choice0 = choices.at(0).toObject();
//    QJsonObject message = choice0.value("message").toObject();
//
//    // Doubao currently returns a plain string in message.content.
//    QString content = message.value("content").toString();
//
//    if (content.isEmpty()) {
//        textEdit_->append(
//            QStringLiteral("\nNo 'message.content' found in response."));
//        return;
//    }
//
//    textEdit_->setMarkdown(content.trimmed());
//    reply->deleteLater();
//    if (promptEdit_) {
//        promptEdit_->clear();
//        promptEdit_->setPlaceholderText(
//            QStringLiteral("Ask any questions"));
//    }
//
//}
//
//// ================== Copy button ==================
//
//void AiDescribeDialog::onCopyTextClicked()
//{
//    textEdit_->selectAll();
//    textEdit_->copy();
//
//    // Or directly:
//    // QGuiApplication::clipboard()->setText(textEdit_->toPlainText());
//}
//void AiDescribeDialog::onGenerateClicked()
//{
//    // 每次点击用当前 prompt 重新问一遍
//    textEdit_->clear();
//    textEdit_->setPlainText(
//        QStringLiteral("Requesting description from AI, please wait...\n"));
//    sendRequest(originalPixmap_);
//}
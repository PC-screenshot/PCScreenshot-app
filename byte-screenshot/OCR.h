#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <memory>

// 前置声明，避免在头文件中包含 Paddle/OpenCV 头文件
namespace cv {
    class Mat;
}

class PaddleOcrInternal;

class OcrEngine : public QObject {
    Q_OBJECT

public:
    explicit OcrEngine(QObject* parent = nullptr);
    ~OcrEngine();

    // 单例访问接口
    static OcrEngine& instance();

    // 初始化 OCR 引擎
    // modelDir: 包含 det, rec, cls 模型文件的目录
    bool init(const QString& modelDir);

    // 提供的公共函数，传入图片进行识别，返回识别结果
    QString detectText(const QImage& image);

    // 释放 OCR 引擎资源
    void release();

private:
    // 内部处理函数：QImage 转 cv::Mat
    static cv::Mat QImageToCvMat(const QImage& image);

    // 使用 Pimpl 模式隐藏 PaddleOCR 具体实现细节
    std::unique_ptr<PaddleOcrInternal> internal_;
    bool is_initialized_ = false;
};
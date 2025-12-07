#include "OCR.h"
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

// #define USE_PADDLE_OCR

#ifdef USE_PADDLE_OCR
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <paddle_inference_api.h>

#include "paddleocr.h"
#include "args.h"

using namespace PaddleOCR;
#endif

// 简单的 cv::Mat 前置定义（如果未启用 OpenCV）
#ifndef USE_PADDLE_OCR
namespace cv {
    class Mat {
    public:
        bool empty() const { return true; }
    };
}
#endif

class PaddleOcrInternal {
public:
    PaddleOcrInternal() {}
    ~PaddleOcrInternal() {}

    bool init(const QString& userModelDir) {
#ifdef USE_PADDLE_OCR
        QString baseDir;
        QString dictPath;

        qDebug() << "OCR Init: Checking user model dir:" << userModelDir;

        // 路径搜索逻辑：
        // 1. 优先检查传入的 userModelDir (通常是运行目录下的 inference)
        if (!userModelDir.isEmpty()) {
            QDir dir(userModelDir);
            if (dir.exists()) {
                // 必须包含三个模型文件夹才算有效
                bool hasDet = dir.exists("det");
                bool hasCls = dir.exists("cls");
                bool hasRec = dir.exists("rec");

                if (hasDet && hasCls && hasRec) {
                    baseDir = userModelDir;
                    
                    // 查找字典文件：优先根目录，其次 rec 目录
                    if (dir.exists("ppocr_keys_v1.txt")) {
                        dictPath = dir.filePath("ppocr_keys_v1.txt");
                    } else if (QFile::exists(dir.filePath("rec/ppocr_keys_v1.txt"))) {
                        dictPath = dir.filePath("rec/ppocr_keys_v1.txt");
                    } else {
                        qDebug() << "OCR Init: Model folders found but dictionary (ppocr_keys_v1.txt) is missing in" << userModelDir;
                    }
                } else {
                    qDebug() << "OCR Init: Found 'inference' dir but missing subfolders. Det:" << hasDet << " Cls:" << hasCls << " Rec:" << hasRec;
                }
            }
        }

        // 2. 如果未找到，尝试查找开发环境的相对路径
        // 假设 exe 位于 x64/Release，源码在 byte-screenshot，依赖在 3rdparty
        if (baseDir.isEmpty()) {
            QString appPath = QCoreApplication::applicationDirPath();
            QString devPaddleRoot = QDir::cleanPath(appPath + "/../../3rdparty/cpp/2.7/PaddleOCR");
            
            if (QDir(devPaddleRoot + "/models").exists()) {
                baseDir = devPaddleRoot + "/models";
                dictPath = devPaddleRoot + "/deploy/cpp_infer/ppocr_keys_v1.txt";
                qDebug() << "OCR Init: Fallback to dev environment path:" << baseDir;
            }
        }

        if (baseDir.isEmpty()) {
            qDebug() << "Error: PaddleOCR models directory not found. Searched in:" << userModelDir;
            return false;
        }
        
        if (dictPath.isEmpty()) {
            qDebug() << "Error: Dictionary file (ppocr_keys_v1.txt) not found.";
            return false;
        }

        qDebug() << "PaddleOCR Model Path:" << baseDir;
        qDebug() << "PaddleOCR Dict Path:" << dictPath;
        
        std::string base_dir_std = QDir::toNativeSeparators(baseDir).toStdString();
        
        FLAGS_det_model_dir = base_dir_std + "\\det";
        FLAGS_cls_model_dir = base_dir_std + "\\cls";
        FLAGS_rec_model_dir = base_dir_std + "\\rec";
        FLAGS_rec_char_dict_path = QDir::toNativeSeparators(dictPath).toStdString();
        
        // 检查模型文件是否存在
        if (!QDir(QString::fromStdString(FLAGS_det_model_dir)).exists()) {
            qDebug() << "Error: Det model dir not found:" << QString::fromStdString(FLAGS_det_model_dir);
            return false;
        }
        if (!QDir(QString::fromStdString(FLAGS_cls_model_dir)).exists()) {
            qDebug() << "Error: Cls model dir not found:" << QString::fromStdString(FLAGS_cls_model_dir);
            return false;
        }
        if (!QDir(QString::fromStdString(FLAGS_rec_model_dir)).exists()) {
            qDebug() << "Error: Rec model dir not found:" << QString::fromStdString(FLAGS_rec_model_dir);
            return false;
        }
        if (!QFile::exists(QString::fromStdString(FLAGS_rec_char_dict_path))) {
            qDebug() << "Error: Char dict not found:" << QString::fromStdString(FLAGS_rec_char_dict_path);
            return false;
        }

        FLAGS_use_gpu = false;
        FLAGS_enable_mkldnn = true; // 禁用 MKLDNN 以避免潜在的兼容性问题
        FLAGS_use_angle_cls = true;
        
        // 内存优化配置
        FLAGS_cpu_threads = 2;       // 限制 CPU 线程数，降低内存占用 (默认可能为 10)
        FLAGS_benchmark = false;     // 关闭 Benchmark 模式
        FLAGS_rec_batch_num = 1;     // 减少批处理数量
        FLAGS_cls_batch_num = 1;
        FLAGS_limit_side_len = 960;  // 限制输入图像长边，避免超大图导致 OOM
        FLAGS_limit_type = "max";
        FLAGS_use_dilation = false;

        qDebug() << "Initializing PPOCR system...";
        // 初始化 PPOCR 对象
        try {
            ocr_system_ = std::make_unique<PPOCR>();
        } catch (const std::exception& e) {
            qDebug() << "Exception during PPOCR init:" << e.what();
            return false;
        } catch (...) {
            qDebug() << "Unknown exception during PPOCR init";
            return false;
        }
        
        qDebug() << "PaddleOCR initialized.";
        return true;
#else
        Q_UNUSED(userModelDir);
        qDebug() << "PaddleOCR disabled. Define USE_PADDLE_OCR to enable.";
        return false;
#endif
    }

    QString detect(const cv::Mat& img) {
#ifdef USE_PADDLE_OCR
        if (img.empty()) return "Error: Empty image";

        if (!ocr_system_) {
            return "Error: OCR System not initialized.";
        }

        // 调用推理接口
        std::vector<OCRPredictResult> ocr_results = ocr_system_->ocr(img, true, true, true);
        
        // 智能文本合并逻辑 (Smart Paragraph Merging) - 用户请求移除，恢复为简单换行
        QString fullText;
        for (const auto& res : ocr_results) {
            if (res.score < 0.5) continue;
            fullText += QString::fromStdString(res.text) + "\n";
        }
        
        if (fullText.isEmpty() && !ocr_results.empty()) {
            return "No text detected (Low confidence).";
        } else if (fullText.isEmpty()) {
            return "No text detected.";
        }
        
        return fullText.trimmed();
#else
        Q_UNUSED(img);
        return "OCR not enabled.";
#endif
    }

    void release() {
#ifdef USE_PADDLE_OCR
        if (ocr_system_) {
            ocr_system_.reset();
            qDebug() << "PaddleOCR resources released to save memory.";
        }
#endif
    }

private:
#ifdef USE_PADDLE_OCR
    std::unique_ptr<PPOCR> ocr_system_;
#endif
};

OcrEngine::OcrEngine(QObject* parent)
    : QObject(parent), internal_(std::make_unique<PaddleOcrInternal>()) {
}

OcrEngine::~OcrEngine() = default;

OcrEngine& OcrEngine::instance() {
    static OcrEngine s_instance;
    return s_instance;
}

bool OcrEngine::init(const QString& modelDir) {
    if (!internal_) return false;
    is_initialized_ = internal_->init(modelDir);
    return is_initialized_;
}

void OcrEngine::release() {
    if (internal_) {
        internal_->release();
    }
    is_initialized_ = false;
}

QString OcrEngine::detectText(const QImage& image) {
    if (image.isNull()) {
        return "Error: Empty Image";
    }

    qDebug() << "OCR Start... Image Size:" << image.size();

    // 图像预处理：如果图片过大，进行缩放以减少内存占用并加速推理
    // 限制长边最大为 1600 (PaddleOCR 默认 limit_side_len=960，但在转换前缩放能减少 QImageToCvMat 的开销)
    QImage processedImg = image;
    int maxSide = 1600;
    if (processedImg.width() > maxSide || processedImg.height() > maxSide) {
        processedImg = processedImg.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        qDebug() << "Image scaled to:" << processedImg.size() << "for performance optimization.";
    }

    // 1. QImage -> cv::Mat
    cv::Mat mat = QImageToCvMat(processedImg);

    // 2. Run Inference
    QString result;
    if (is_initialized_) {
        result = internal_->detect(mat);
    } else {
        // 尝试默认初始化（如果未手动调用 init）
        // 优先使用运行目录下的 inference 目录
        QString defaultModelPath = QCoreApplication::applicationDirPath() + "/inference";
        if (init(defaultModelPath)) {
            result = internal_->detect(mat);
        } else {
            result = "OCR Engine init failed.\nCheck ./inference folder for models and keys.";
        }
    }

    // 3. 立即释放资源以降低内存占用 (Stateless Mode)
    release();

    return result;
}

cv::Mat OcrEngine::QImageToCvMat(const QImage& image) {
#ifdef USE_PADDLE_OCR
    cv::Mat mat;
    cv::Mat matClone;
    switch (image.format()) {
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
        mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.constBits(), image.bytesPerLine());
        // PaddleOCR 通常需要 3 通道 BGR
        cv::cvtColor(mat, matClone, cv::COLOR_BGRA2BGR);
        break;
    case QImage::Format_RGB888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
        // QImage RGB888 是 R,G,B -> OpenCV 需要 B,G,R
        cv::cvtColor(mat, matClone, cv::COLOR_RGB2BGR);
        break;
    case QImage::Format_Grayscale8:
        mat = cv::Mat(image.height(), image.width(), CV_8UC1, (void*)image.constBits(), image.bytesPerLine());
        cv::cvtColor(mat, matClone, cv::COLOR_GRAY2BGR);
        break;
    default:
        QImage tmp = image.convertToFormat(QImage::Format_RGB888);
        mat = cv::Mat(tmp.height(), tmp.width(), CV_8UC3, (void*)tmp.constBits(), tmp.bytesPerLine());
        cv::cvtColor(mat, matClone, cv::COLOR_RGB2BGR);
        break;
    }
    return matClone;
#else
    Q_UNUSED(image);
    return cv::Mat();
#endif
}

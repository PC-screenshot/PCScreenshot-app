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
#include <algorithm>
#include <QRect>

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

    // Paddle Model Path Check
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

        // OCR Engine Settings
        FLAGS_use_gpu = false;
        FLAGS_enable_mkldnn = true; 
        FLAGS_use_angle_cls = true;  // 纠正旋转文本
        
        // 减少线程数以降低内存峰值
        FLAGS_cpu_threads = 1;       // 限制为单线程
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
        std::vector<OCRPredictResult> ocr_results = ocr_system_->ocr(img, true, true, false);
        
        // 更新段落合并方法
        QString fullText;
        qDebug() << "Pallde Results:";
        if (!ocr_results.empty()) {
            // 1. 过滤低置信度结果 & 转换为内部结构
            struct OcrBox {
                QRect rect;
                QString text;
            };
            std::vector<OcrBox> boxes;
            
            for (const auto& res : ocr_results) {
                if (res.score >= 0.5) {
                     // Paddle box: [0]=TL, [1]=TR, [2]=BR, [3]=BL
                     // 计算包围盒
                     int x_coords[] = {res.box[0][0], res.box[1][0], res.box[2][0], res.box[3][0]};
                     int y_coords[] = {res.box[0][1], res.box[1][1], res.box[2][1], res.box[3][1]};
                     
                     int x_min = x_coords[0], x_max = x_coords[0];
                     int y_min = y_coords[0], y_max = y_coords[0];
                     for(int k=1; k<4; k++) {
                         if(x_coords[k] < x_min) x_min = x_coords[k];
                         if(x_coords[k] > x_max) x_max = x_coords[k];
                         if(y_coords[k] < y_min) y_min = y_coords[k];
                         if(y_coords[k] > y_max) y_max = y_coords[k];
                     }
                     
                     QString text = QString::fromStdString(res.text);
                     boxes.push_back({QRect(QPoint(x_min, y_min), QPoint(x_max, y_max)), text});
                     
                     qDebug() << "PADDLE Text:" << text 
                              << "Score:" << res.score
                              << "Rect:" << x_min << y_min << x_max << y_max;
                }
            }

            // 先按 Top 坐标排序，保证处理顺序大致从上到下
            std::sort(boxes.begin(), boxes.end(), [](const OcrBox& a, const OcrBox& b) {
                return a.rect.top() < b.rect.top();
            });

            struct Row {
                int top, bottom;
                std::vector<OcrBox> items;
                
                // 更新行的边界
                void add(const OcrBox& b) {
                    items.push_back(b);
                    // 动态更新行的上下界，取并集
                    top = std::min(top, b.rect.top());
                    bottom = std::max(bottom, b.rect.bottom());
                }
                
                // 判断是否属于同一行（基于垂直重叠）
                bool isSameLine(const OcrBox& b) {
                    int overlapStart = std::max(top, b.rect.top());
                    int overlapEnd = std::min(bottom, b.rect.bottom());
                    int overlapHeight = overlapEnd - overlapStart;
                    
                    if (overlapHeight <= 0) return false;
                    
                    int rowH = bottom - top;
                    int boxH = b.rect.height();
                    // 重叠高度超过较小高度的一半，视为同一行
                    return overlapHeight > (std::min(rowH, boxH) * 0.5);
                }
            };
            
            std::vector<Row> rows;
            for (const auto& box : boxes) {
                bool added = false;
                // 尝试加入当前最后一行
                if (!rows.empty() && rows.back().isSameLine(box)) {
                    rows.back().add(box);
                    added = true;
                }
                
                if (!added) {
                    rows.push_back({box.rect.top(), box.rect.bottom(), {box}});
                }
            }
            
            qDebug() << "Rows Detected:" << rows.size();

            // 3. 逐行合并文本
            for (size_t r = 0; r < rows.size(); ++r) {
                auto& row = rows[r];
                
                // 行内按 X 坐标排序 (从左到右)
                std::sort(row.items.begin(), row.items.end(), [](const OcrBox& a, const OcrBox& b) {
                    return a.rect.left() < b.rect.left();
                });

                for (size_t i = 0; i < row.items.size(); ++i) {
                    const auto& curr = row.items[i];
                    fullText += curr.text;

                    if (i < row.items.size() - 1) {
                         const auto& next = row.items[i + 1];
                         // 检查水平间距，决定是否加空格
                         int dist = next.rect.left() - curr.rect.right();
                         // 简单的空格策略：如果间距大于0，加空格（或者是代码中的自然间隔）
                         // 对于代码场景，通常都需要空格分隔
                         fullText += " ";
                    }
                }
                
                // 换行
                if (r < rows.size() - 1) {
                    fullText += "\n"; 
                }
            }
        }
        
        if (fullText.isEmpty() && !ocr_results.empty()) {
            return "No text detected (Low confidence).";
        } else if (fullText.isEmpty()) {
            return "No text detected.";
        }
        
        QString finalResult = fullText.trimmed();
        qDebug() << finalResult;
        
        return finalResult;
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

#include <QElapsedTimer>

QString OcrEngine::detectText(const QImage& image) {
    if (image.isNull()) {
        return "Error: Empty Image";
    }

    QElapsedTimer timer;
    timer.start();

    //qDebug() << Image Size: << image.size();

    // 1. QImage -> cv::Mat
    cv::Mat mat = QImageToCvMat(image);

    // 2. 推理
    QString result;
    if (is_initialized_) {
        result = internal_->detect(mat);
    } else {
        QString defaultModelPath = QCoreApplication::applicationDirPath() + "/inference";
        if (init(defaultModelPath)) {
            result = internal_->detect(mat);
        } else {
            result = "OCR Engine init failed.\nCheck ./inference folder for models and keys.";
        }
    }

    // 降低内存占用
    release();
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "OCR Total Time Cost:" << elapsed << "ms";

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

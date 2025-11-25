#include "EditorWindow.h"

#include <QVBoxLayout>
#include <QLabel>

EditorWindow::EditorWindow(const QPixmap& pixmap, QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Screenshot Editor");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    image_label_ = new QLabel(this);
    image_label_->setPixmap(pixmap);
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setStyleSheet("background-color: #202020;");
    layout->addWidget(image_label_);

    toolbar_ = new EditorToolbar(this);
    toolbar_->setFixedHeight(40);
    layout->addWidget(toolbar_);

    resize(pixmap.width() + 40, pixmap.height() + 80);
}

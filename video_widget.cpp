#include "video_widget.h"

#include <QPainter>

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{
    // 设置黑色背景
    setAutoFillBackground(true);
    setPalette(QPalette(Qt::black));
}

void VideoWidget::setImage(const QImage &image)
{
    QMutexLocker locker(&m_mutex);
    m_image = image;
    update();  // 触发重绘
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QMutexLocker locker(&m_mutex);
    if (!m_image.isNull())
    {
        // 保持宽高比缩放图像
        QImage scaled = m_image.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        painter.drawImage(QPoint(x, y), scaled);
    }
    else
    {
        // 没有图像时显示提示文字
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, QString("未打开视频文件"));
    }
}

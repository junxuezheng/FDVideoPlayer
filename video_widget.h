#ifndef VIDEO_WIDGET_H
#define VIDEO_WIDGET_H


#include <QWidget>
#include <QPaintEvent>
#include <QImage>
#include <QMutex>

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);

    // 接收从解码线程传来的图像
    void setImage(const QImage &image);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_image;
    QMutex m_mutex;  // 保护图像数据
};

#endif // VIDEO_WIDGET_H

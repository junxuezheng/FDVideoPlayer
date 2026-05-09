#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_isSliderPressed(false)
    , m_duration(0)
    , m_currentPosition(0)
{
    setupUI();

    // 创建解码线程
//    m_thread = new VideoThread(this);
    m_thread = new VideoThread();

    // 方式1：直接设置样式表
    this->setStyleSheet("QMainWindow { background-color: lightblue; }");

    // 连接信号槽
    connect(m_thread, &VideoThread::frameReady, this, &MainWindow::onFrameReceived);
    connect(m_thread, &VideoThread::durationChanged, this, &MainWindow::onDurationChanged);
    connect(m_thread, &VideoThread::positionChanged, this, &MainWindow::onPositionChanged);
    connect(m_thread, &VideoThread::playbackFinished, this, &MainWindow::onPlaybackFinished);
    connect(m_thread, &VideoThread::errorOccurred, this, &MainWindow::onError);
}

MainWindow::~MainWindow()
{
    m_thread->stop();
    m_thread->wait();
    delete  ui;
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("FDVideoPlayer"));//Qt + FFmpeg + SDL3 视频播放器
    resize(800, 600);

    // 创建中央部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 创建布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 视频显示区域
    m_videoWidget = new VideoWidget(this);
    m_videoWidget->setMinimumSize(320, 240);
    mainLayout->addWidget(m_videoWidget,10);

    // 控制区域
    QHBoxLayout *controlLayout = new QHBoxLayout();

    m_openBtn = new QPushButton(QStringLiteral("打开"), this);
    m_playPauseBtn = new QPushButton(QStringLiteral("播放"), this);
    m_playPauseBtn->setEnabled(false);
    m_stopBtn = new QPushButton(QStringLiteral("停止"), this);
    m_stopBtn->setEnabled(false);

    controlLayout->addWidget(m_openBtn);
    controlLayout->addWidget(m_playPauseBtn);
    controlLayout->addWidget(m_stopBtn);

    // 进度条
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setEnabled(false);
    m_slider->setRange(0, 1000);  // 使用0-1000的相对范围

    m_timeLabel = new QLabel(QStringLiteral("00:00 / 00:00"), this);

    controlLayout->addWidget(m_slider);
    controlLayout->addWidget(m_timeLabel);

    mainLayout->addLayout(controlLayout,1);

    // 连接信号
    connect(m_openBtn, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_slider, &QSlider::sliderPressed, this, &MainWindow::onSliderPressed);
    connect(m_slider, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);
    connect(m_slider, &QSlider::valueChanged, this, &MainWindow::onSliderValueChanged);
}

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("选择视频文件"), "",
        QStringLiteral("视频文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv *.rmvb *.3gp);;所有文件 (*.*)"));

    if (fileName.isEmpty())
    {
        return;
    }

    m_currentFile = fileName;

    // 停止当前播放
    m_thread->stop();
    m_thread->wait();
    m_thread->clear();

    m_thread->init();

    // 更新UI状态
    m_playPauseBtn->setText(QStringLiteral("暂停"));
    m_playPauseBtn->setEnabled(true);
    m_stopBtn->setEnabled(true);
    m_slider->setEnabled(true);

    // 打开新文件
    m_thread->openFile(fileName);

    m_isStop=false;
}

void MainWindow::onPlayPause()
{
    if (m_playPauseBtn->text() == QStringLiteral("暂停"))
    {
        m_thread->pause(true);
        m_playPauseBtn->setText(QStringLiteral("播放"));
    }
    else
    {
        if(m_thread->isRunning())
        {
            m_thread->pause(false);
            m_playPauseBtn->setText(QStringLiteral("暂停"));
        }
    }
}

void MainWindow::onStop()
{
    m_isStop=true;

    m_thread->stop();
    m_thread->wait();

    m_thread->init();

    QImage(image);
    m_videoWidget->setImage(image);

    m_playPauseBtn->setText(QStringLiteral("播放"));
    m_slider->setValue(0);
    m_timeLabel->setText("00:00 / 00:00");
}

void MainWindow::onSliderPressed()
{
    m_isSliderPressed = true;
}

void MainWindow::onSliderReleased()
{
    m_isSliderPressed = false;
    // 这里可以实现跳转功能（需要在VideoThread中添加seek支持）
}

void MainWindow::onSliderValueChanged(int value)
{
    if (!m_isSliderPressed) return;

    // 预览位置
    if (m_duration > 0)
    {
        qint64 pos = value * m_duration / 1000;
        QTime time(0, 0, 0);
        QString timeStr = time.addMSecs(pos).toString(QStringLiteral("mm:ss"));
        QString totalStr = time.addMSecs(m_duration).toString(QStringLiteral("mm:ss"));
        m_timeLabel->setText(QString("%1 / %2").arg(timeStr, totalStr));
    }
}

void MainWindow::onFrameReceived(const QImage &image)
{
    if(m_isStop)
        return;
    m_videoWidget->setImage(image);
}

void MainWindow::onDurationChanged(qint64 duration)
{
    m_duration = duration;
    QTime time(0, 0, 0);
    QString totalStr = time.addMSecs(duration).toString("mm:ss");
    QString currentStr = time.addMSecs(m_currentPosition).toString("mm:ss");
    m_timeLabel->setText(QString("%1 / %2").arg(currentStr, totalStr));
}

void MainWindow::onPositionChanged(qint64 position)
{
    m_currentPosition = position;

    if (!m_isSliderPressed && m_duration > 0)
    {
        int value = position * 1000 / m_duration;
        m_slider->setValue(value);

        QTime time(0, 0, 0);
        QString currentStr = time.addMSecs(position).toString("mm:ss");
        QString totalStr = time.addMSecs(m_duration).toString("mm:ss");
        m_timeLabel->setText(QString("%1 / %2").arg(currentStr, totalStr));
    }
}

void MainWindow::onPlaybackFinished()
{
    m_playPauseBtn->setText(QStringLiteral("播放"));
    m_slider->setValue(0);
    m_timeLabel->setText(QStringLiteral("00:00 / 00:00"));
}

void MainWindow::onError(const QString &err)
{
    QMessageBox::critical(this, QStringLiteral("播放错误"), err);
}

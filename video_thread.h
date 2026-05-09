#ifndef VIDEO_THREAD_H
#define VIDEO_THREAD_H

#include <QThread>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>

// SDL3 头文件
#include <SDL3/SDL.h>

// FFmpeg 头文件
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
}

// 音视频帧队列节点
struct Frame {
    AVFrame *frame;
    double pts;           // 显示时间戳（秒）
    double duration;      // 持续时间（秒）
};

class VideoThread : public QThread
{
    Q_OBJECT
public:
    explicit VideoThread(QObject *parent = nullptr);
    ~VideoThread();

    void clear();
    void openFile(const QString &filePath);
    void init();
    void stop();
    void pause(bool p);

signals:
    void frameReady(const QImage &image);
    void durationChanged(qint64 duration);
    void positionChanged(qint64 position);
    void playbackFinished();
    void errorOccurred(const QString &err);

protected:
    void run() override;

private:
    void clearQueues();
    bool initVideoDecoder();
    bool initAudioDecoder();
    bool initSwrContext();          // 仍然需要，用于将音频转换为固定格式
    void decodeLoop();
    void initAudioOutput();         // 使用 SDL3 音频流初始化
    void playAudio();               // 使用 SDL3 音频流播放
    double getAudioClock();
    void enqueueVideoFrame(AVFrame *frame, double pts);
    void enqueueAudioFrame(AVFrame *frame, double pts, double duration);
    Frame* dequeueVideoFrame();
    Frame* dequeueAudioFrame();

private:
    QString m_filePath;
    volatile bool m_stop;
    volatile bool m_pause;
    QMutex m_pauseMutex;
    QWaitCondition m_pauseCond;

    // FFmpeg 核心
    AVFormatContext *m_formatCtx;
    int m_videoStreamIndex;
    int m_audioStreamIndex;
    AVCodecContext *m_videoCodecCtx;
    AVCodecContext *m_audioCodecCtx;
    const AVCodec *m_videoCodec;
    const AVCodec *m_audioCodec;
    AVRational m_videoTimeBase;
    AVRational m_audioTimeBase;

    // 视频相关
    SwsContext *m_swsCtx;
    AVFrame *m_videoFrame;
    AVFrame *m_rgbFrame;
    uint8_t *m_rgbBuffer;
    int m_videoWidth;
    int m_videoHeight;

    // 音频相关
    SwrContext *m_swrCtx;
    AVFrame *m_audioFrame;
    int m_audioSampleRate;
    int m_audioChannels;
    AVSampleFormat m_audioSampleFmt;

    // SDL3 音频
    SDL_AudioDeviceID m_audioDeviceId;  // 音频设备 ID
    SDL_AudioStream *m_audioStream;     // 音频流对象

    // 音视频队列
    QQueue<Frame*> m_videoQueue;
    QQueue<Frame*> m_audioQueue;
    QMutex m_videoQueueMutex;
    QMutex m_audioQueueMutex;
    QWaitCondition m_videoQueueCond;
    QWaitCondition m_audioQueueCond;
    int m_videoQueueMaxSize;
    int m_audioQueueMaxSize;

    // 同步相关
    double m_audioClock;
    double m_videoPts;
    QMutex m_clockMutex;

    // 播放控制
    qint64 m_duration;
};

#endif // VIDEO_THREAD_H

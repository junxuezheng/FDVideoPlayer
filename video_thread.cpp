#include "video_thread.h"
#include <QDebug>
#include <QFileInfo>

VideoThread::VideoThread(QObject *parent) : QThread(parent)
{
    m_stop = false;
    m_pause = false;
    m_formatCtx = nullptr;
    m_videoCodecCtx = nullptr;
    m_audioCodecCtx = nullptr;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_swsCtx = nullptr;
    m_swrCtx = nullptr;
    m_videoFrame = nullptr;
    m_audioFrame = nullptr;
    m_rgbFrame = nullptr;
    m_rgbBuffer = nullptr;
    m_audioDeviceId = 0;
    m_audioStream = nullptr;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_audioClock = 0.0;
    m_videoPts = 0.0;
    m_videoQueueMaxSize = 20;
    m_audioQueueMaxSize = 20;
    m_duration = 0;
}

VideoThread::~VideoThread()
{
    stop();
    wait();
    clear();
}

void VideoThread::clear()
{
    if (m_rgbBuffer) av_free(m_rgbBuffer);
    if (m_rgbFrame) av_frame_free(&m_rgbFrame);
    if (m_videoFrame) av_frame_free(&m_videoFrame);
    if (m_audioFrame) av_frame_free(&m_audioFrame);
    if (m_swsCtx) sws_freeContext(m_swsCtx);
    if (m_swrCtx) swr_free(&m_swrCtx);
    if (m_videoCodecCtx) avcodec_free_context(&m_videoCodecCtx);
    if (m_audioCodecCtx) avcodec_free_context(&m_audioCodecCtx);
    if (m_formatCtx) avformat_close_input(&m_formatCtx);

    // 关闭 SDL 音频流和设备
    if (m_audioStream) {
        SDL_DestroyAudioStream(m_audioStream);
        m_audioStream = nullptr;
    }
    if (m_audioDeviceId != 0) {
        SDL_CloseAudioDevice(m_audioDeviceId);
        m_audioDeviceId = 0;
    }
    SDL_Quit();
}

void VideoThread::clearQueues()
{
    QMutexLocker videoLocker(&m_videoQueueMutex);
    QMutexLocker audioLocker(&m_audioQueueMutex);

    while (!m_videoQueue.isEmpty()) {
        Frame *f = m_videoQueue.dequeue();
        av_frame_free(&f->frame);
        delete f;
    }
    while (!m_audioQueue.isEmpty()) {
        Frame *f = m_audioQueue.dequeue();
        av_frame_free(&f->frame);
        delete f;
    }
}

void VideoThread::openFile(const QString &filePath)
{
    m_filePath = filePath;
    if (!isRunning()) {
        start();
    }
}

void VideoThread::init()
{
    m_stop = false;
    m_pause = false;
    m_pauseCond.wakeAll();
}

void VideoThread::stop()
{
    m_stop = true;
    m_pause = false;
    m_pauseCond.wakeAll();
    m_videoQueueCond.wakeAll();
    m_audioQueueCond.wakeAll();
}

void VideoThread::pause(bool p)
{
    if (p) {
        QMutexLocker locker(&m_pauseMutex);
        m_pause = true;
        // 立即暂停 SDL 音频设备
        if (m_audioDeviceId != 0) {
            SDL_PauseAudioDevice(m_audioDeviceId);
        }
    } else {
        QMutexLocker locker(&m_pauseMutex);
        m_pause = false;
        m_pauseCond.wakeAll();
        // 恢复 SDL 音频设备
        if (m_audioDeviceId != 0) {
            SDL_ResumeAudioDevice(m_audioDeviceId);
        }
    }
}

void VideoThread::run()
{
    qDebug() << "run Thread ID:" << QThread::currentThreadId();

    // 1. 打开输入文件
    int ret = avformat_open_input(&m_formatCtx, m_filePath.toStdString().c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        emit errorOccurred(QString("无法打开文件: %1").arg(errbuf));
        return;
    }

    // 2. 查找流信息
    ret = avformat_find_stream_info(m_formatCtx, nullptr);
    if (ret < 0) {
        emit errorOccurred("无法找到流信息");
        return;
    }

    // 3. 获取总时长
    if (m_formatCtx->duration != AV_NOPTS_VALUE) {
        m_duration = m_formatCtx->duration / AV_TIME_BASE * 1000;
        emit durationChanged(m_duration);
    }

    // 4. 初始化解码器
    bool hasVideo = initVideoDecoder();
    bool hasAudio = initAudioDecoder();

    if (!hasVideo && !hasAudio) {
        emit errorOccurred("没有有效的视频或音频流");
        return;
    }

    // 5. 初始化 SDL 音频子系统
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        emit errorOccurred(QString("SDL 初始化失败: %1").arg(SDL_GetError()));
        return;
    }

    // 6. 初始化音频输出设备和流
    if (hasAudio) {
        initAudioOutput();
        if (!initSwrContext()) {
            emit errorOccurred("初始化音频重采样器失败");
            return;
        }
        // 启动音频设备播放
        SDL_ResumeAudioDevice(m_audioDeviceId);
    }

    // 7. 启动解码线程
    QThread *decodeThread = nullptr;
    if (hasVideo || hasAudio) {
        decodeThread = QThread::create([this]() { decodeLoop(); });
        decodeThread->start();
    }

    // 8. 启动音频播放线程
    QThread *audioThread = nullptr;
    if (hasAudio) {
        audioThread = QThread::create([this]() { playAudio(); });
        audioThread->start();
    }

    // 9. 视频显示主循环
    while (!m_stop) {
        // 暂停处理
        {
            QMutexLocker locker(&m_pauseMutex);
            if (m_pause) {
                m_pauseCond.wait(&m_pauseMutex);
            }
        }

        Frame *f = dequeueVideoFrame();
        if (!f) {
            QThread::msleep(10);
            continue;
        }

        // 音视频同步（以音频时钟为基准）
        double audioClock = getAudioClock();
        double diff = f->pts - audioClock;

        if (diff > 0.05) {
            int waitMs = qBound(1, static_cast<int>(diff * 1000), 100);
            QThread::msleep(waitMs);
        } else if (diff < -0.05) {
            // 视频落后太多，丢帧
            av_frame_free(&f->frame);
            delete f;
            continue;
        }

        // YUV → RGB 并显示
        if (f->frame->width > 0 && f->frame->height > 0) {
            sws_scale(m_swsCtx,
                      f->frame->data, f->frame->linesize,
                      0, f->frame->height,
                      m_rgbFrame->data, m_rgbFrame->linesize);

            QImage image(m_rgbBuffer, m_videoWidth, m_videoHeight, QImage::Format_RGB888);
            emit frameReady(image.copy());

            if (f->pts >= 0) {
                emit positionChanged(static_cast<qint64>(f->pts * 1000));
            }
        }

        av_frame_free(&f->frame);
        delete f;
        QThread::msleep(1);
    }

    // 10. 等待子线程结束
    if (decodeThread) {
        decodeThread->quit();
        decodeThread->wait();
        delete decodeThread;
    }
    if (audioThread) {
        audioThread->quit();
        audioThread->wait();
        delete audioThread;
    }

    // 清理
    clearQueues();
    if (m_audioDeviceId != 0) {
        SDL_PauseAudioDevice(m_audioDeviceId);
    }

    emit playbackFinished();
}

bool VideoThread::initVideoDecoder()
{
    // 查找视频流
    for (unsigned int i = 0; i < m_formatCtx->nb_streams; i++) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }
    if (m_videoStreamIndex == -1) return false;

    AVStream *videoStream = m_formatCtx->streams[m_videoStreamIndex];
    m_videoTimeBase = videoStream->time_base;

    AVCodecParameters *codecParams = videoStream->codecpar;
    m_videoCodec = avcodec_find_decoder(codecParams->codec_id);
    if (!m_videoCodec) {
        emit errorOccurred("找不到视频解码器");
        return false;
    }

    m_videoCodecCtx = avcodec_alloc_context3(m_videoCodec);
    if (!m_videoCodecCtx) {
        emit errorOccurred("无法分配视频解码器上下文");
        return false;
    }

    if (avcodec_parameters_to_context(m_videoCodecCtx, codecParams) < 0) {
        emit errorOccurred("无法复制视频编码参数");
        return false;
    }

    if (avcodec_open2(m_videoCodecCtx, m_videoCodec, nullptr) < 0) {
        emit errorOccurred("无法打开视频解码器");
        return false;
    }

    m_videoWidth = m_videoCodecCtx->width;
    m_videoHeight = m_videoCodecCtx->height;

    m_videoFrame = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_videoWidth, m_videoHeight, 1);
    m_rgbBuffer = (uint8_t *)av_malloc(numBytes);
    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize, m_rgbBuffer,
                         AV_PIX_FMT_RGB24, m_videoWidth, m_videoHeight, 1);

    m_swsCtx = sws_getContext(
        m_videoWidth, m_videoHeight, m_videoCodecCtx->pix_fmt,
        m_videoWidth, m_videoHeight, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    return true;
}

bool VideoThread::initAudioDecoder()
{
    for (unsigned int i = 0; i < m_formatCtx->nb_streams; i++) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_audioStreamIndex = i;
            break;
        }
    }
    if (m_audioStreamIndex == -1) {
        emit errorOccurred("未找到音频流");
        return false;
    }

    AVStream *audioStream = m_formatCtx->streams[m_audioStreamIndex];
    m_audioTimeBase = audioStream->time_base;

    AVCodecParameters *codecParams = audioStream->codecpar;
    m_audioCodec = avcodec_find_decoder(codecParams->codec_id);
    if (!m_audioCodec) {
        emit errorOccurred("找不到音频解码器");
        return false;
    }

    m_audioCodecCtx = avcodec_alloc_context3(m_audioCodec);
    if (!m_audioCodecCtx) {
        emit errorOccurred("无法分配音频解码器上下文");
        return false;
    }

    if (avcodec_parameters_to_context(m_audioCodecCtx, codecParams) < 0) {
        emit errorOccurred("无法复制音频编码参数");
        return false;
    }

    if (avcodec_open2(m_audioCodecCtx, m_audioCodec, nullptr) < 0) {
        emit errorOccurred("无法打开音频解码器");
        return false;
    }

    m_audioSampleRate = m_audioCodecCtx->sample_rate;
    m_audioChannels = m_audioCodecCtx->ch_layout.nb_channels;
    m_audioSampleFmt = m_audioCodecCtx->sample_fmt;

    m_audioFrame = av_frame_alloc();
    return true;
}

void VideoThread::initAudioOutput()
{

    // 定义我们提供给 SDL 音频流的源音频格式
    // 我们将在 swr_ctx 中将音频转换为 S16, 立体声, 与解码器相同的采样率
    // 这样 SDL 音频流只需处理格式转换到硬件
    SDL_AudioSpec srcSpec;
    SDL_zero(srcSpec);
    srcSpec.format = SDL_AUDIO_S16;          // 有符号 16 位
    srcSpec.channels = 2;                    // 立体声
    srcSpec.freq = m_audioSampleRate > 0 ? m_audioSampleRate : 44100;

    // 根据 FFmpeg 的采样格式转换为 SDL 格式
//    switch (m_audioSampleFmt)
//    {
//    case AV_SAMPLE_FMT_U8:
//    case AV_SAMPLE_FMT_U8P:
//        srcSpec.format = SDL_AUDIO_U8;
//        break;
//    case AV_SAMPLE_FMT_S16:
//    case AV_SAMPLE_FMT_S16P:
//        srcSpec.format = SDL_AUDIO_S16LE;       // 小端序
//        break;
//    case AV_SAMPLE_FMT_S32:
//    case AV_SAMPLE_FMT_S32P:
//        srcSpec.format = SDL_AUDIO_S32LE;
//        break;
//    case AV_SAMPLE_FMT_FLT:
//    case AV_SAMPLE_FMT_FLTP:
//        srcSpec.format = SDL_AUDIO_F32LE;
//        break;
//    default:
//        emit errorOccurred(QString("不支持的音频格式: %1").arg(m_audioSampleFmt));
//    }

    // 创建音频流，让 SDL 自动处理到硬件设备的格式转换
    m_audioStream = SDL_CreateAudioStream(&srcSpec, nullptr);
    if (!m_audioStream) {
        emit errorOccurred(QString("无法创建 SDL 音频流: %1").arg(SDL_GetError()));
        return;
    }

    // 打开默认音频设备（不指定期望格式，直接使用设备原生格式）
    m_audioDeviceId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (m_audioDeviceId == 0) {
        emit errorOccurred(QString("无法打开 SDL 音频设备: %1").arg(SDL_GetError()));
        return;
    }

    // 将音频流绑定到设备
    if (!SDL_BindAudioStream(m_audioDeviceId, m_audioStream)) {
        emit errorOccurred(QString("无法绑定音频流到设备: %1").arg(SDL_GetError()));
        return;
    }

    qDebug() << "SDL Audio Output Initialized with source:"
             << "freq=" << srcSpec.freq
             << "format=" << SDL_GetAudioFormatName(srcSpec.format)
             << "channels=" << (int)srcSpec.channels;
}

bool VideoThread::initSwrContext()
{
    if (!m_audioCodecCtx) return false;

    SwrContext *swr = swr_alloc();
    if (!swr) return false;

    // 输入参数：解码器输出格式
    av_opt_set_chlayout(swr, "in_chlayout", &m_audioCodecCtx->ch_layout, 0);
    av_opt_set_int(swr, "in_sample_rate", m_audioSampleRate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", m_audioSampleFmt, 0);

    // 输出参数：固定为 S16 立体声，采样率与输入相同（由 SDL 流处理采样率转换）
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, 2);  // 立体声
    av_opt_set_chlayout(swr, "out_chlayout", &outLayout, 0);
    av_opt_set_int(swr, "out_sample_rate", m_audioSampleRate, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(swr) < 0) {
        swr_free(&swr);
        return false;
    }

    m_swrCtx = swr;
    return true;
}

void VideoThread::decodeLoop()
{
    qDebug() << "decodeLoop Thread ID:" << QThread::currentThreadId();

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        emit errorOccurred("无法分配数据包");
        return;
    }

    while (!m_stop) {
        {
            QMutexLocker locker(&m_pauseMutex);
            if (m_pause) {
                m_pauseCond.wait(&m_pauseMutex);
            }
        }

        int ret = av_read_frame(m_formatCtx, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                while ((!m_videoQueue.isEmpty() || !m_audioQueue.isEmpty()) && !m_stop) {
                    QThread::msleep(10);
                }
                break;
            }
            continue;
        }

        if (packet->stream_index == m_videoStreamIndex) {
            ret = avcodec_send_packet(m_videoCodecCtx, packet);
            if (ret == 0) {
                while (avcodec_receive_frame(m_videoCodecCtx, m_videoFrame) == 0) {
                    if (m_stop) break;

                    double pts = 0.0;
                    if (m_videoFrame->pts != AV_NOPTS_VALUE) {
                        pts = m_videoFrame->pts * av_q2d(m_videoTimeBase);
                    }

                    double audioClock = getAudioClock();
                    if (pts < audioClock - 0.01) {
                        av_frame_unref(m_videoFrame);
                        continue;
                    }

                    enqueueVideoFrame(m_videoFrame, pts);
                    av_frame_unref(m_videoFrame);
                }
            }
        } else if (packet->stream_index == m_audioStreamIndex) {
            ret = avcodec_send_packet(m_audioCodecCtx, packet);
            if (ret == 0) {
                while (avcodec_receive_frame(m_audioCodecCtx, m_audioFrame) == 0) {
                    if (m_stop) break;

                    double pts = 0.0;
                    if (m_audioFrame->pts != AV_NOPTS_VALUE) {
                        pts = m_audioFrame->pts * av_q2d(m_audioTimeBase);
                    }
                    double duration = (double)m_audioFrame->nb_samples / m_audioSampleRate;
                    enqueueAudioFrame(m_audioFrame, pts, duration);
                    av_frame_unref(m_audioFrame);
                }
            }
        }
        av_packet_unref(packet);
    }

    av_packet_free(&packet);
}

void VideoThread::playAudio()
{
    qDebug() << "playAudio Thread ID:" << QThread::currentThreadId();

    while (!m_stop) {
        {
            QMutexLocker locker(&m_pauseMutex);
            if (m_pause) {
                m_pauseCond.wait(&m_pauseMutex);
            }
        }

        Frame *f = dequeueAudioFrame();
        if (!f) {
            QThread::msleep(5);
            continue;
        }

        // 更新音频时钟
        {
            QMutexLocker locker(&m_clockMutex);
            m_audioClock = f->pts;
        }

        // 重采样到 S16 立体声（格式固定，由 initSwrContext 保证）
        int outChannels = 2;
        AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_S16;
        int outSampleRate = m_audioSampleRate;  // 保持原采样率，SDL 流会处理

        // 计算重采样后的样本数
        int64_t outSamples = av_rescale_rnd(
            swr_get_delay(m_swrCtx, m_audioSampleRate) + f->frame->nb_samples,
            outSampleRate,
            m_audioSampleRate,
            AV_ROUND_UP);

        // 分配输出缓冲区
        uint8_t **dstData = nullptr;
        int dstLinesize = 0;
        int ret = av_samples_alloc_array_and_samples(
            &dstData, &dstLinesize, outChannels,
            outSamples, outSampleFmt, 0);
        if (ret < 0)
        {
            av_frame_free(&f->frame);
            delete f;
            continue;
        }

        // 执行重采样
        int samplesOut = swr_convert(m_swrCtx, dstData, outSamples,
                                     (const uint8_t **)f->frame->data,
                                     f->frame->nb_samples);

        if (samplesOut > 0)
        {
            int dataSize = av_samples_get_buffer_size(
                &dstLinesize, outChannels, samplesOut,
                outSampleFmt, 1);

            // 将音频数据送入 SDL 音频流
            if (!SDL_PutAudioStreamData(m_audioStream, dstData[0], dataSize)) {
                qWarning() << "SDL_PutAudioStreamData failed:" << SDL_GetError();
            }
        }

        av_freep(&dstData[0]);
        av_freep(&dstData);
        av_frame_free(&f->frame);
        delete f;

        // 稍微延迟避免空转
        QThread::msleep(1);
    }
}

double VideoThread::getAudioClock()
{
//    QMutexLocker locker(&m_clockMutex);
//    return m_audioClock;

    QMutexLocker locker(&m_clockMutex);
    if (!m_audioStream) return m_audioClock;

    // 获取流中尚未播放的字节数
    int queued = SDL_GetAudioStreamQueued(m_audioStream);
    int bytesPerSample = 2 * 2;  // 根据实际格式：16bit 立体声
    double latency = (double)queued / bytesPerSample / m_audioSampleRate;

    // 已播放时间 = 基准 PTS - 延迟（因为基准 PTS 对应送入时刻，数据还在缓冲中未播放）
    return m_audioClock - latency;
}

void VideoThread::enqueueVideoFrame(AVFrame *frame, double pts)
{
    QMutexLocker locker(&m_videoQueueMutex);
    while (m_videoQueue.size() >= m_videoQueueMaxSize && !m_stop) {
        m_videoQueueCond.wait(&m_videoQueueMutex);
    }
    if (m_stop) return;

    Frame *f = new Frame();
    f->frame = av_frame_clone(frame);
    f->pts = pts;
    f->duration = 0;
    m_videoQueue.enqueue(f);
}

void VideoThread::enqueueAudioFrame(AVFrame *frame, double pts, double duration)
{
    QMutexLocker locker(&m_audioQueueMutex);
    while (m_audioQueue.size() >= m_audioQueueMaxSize && !m_stop) {
        m_audioQueueCond.wait(&m_audioQueueMutex);
    }
    if (m_stop) return;

    Frame *f = new Frame();
    f->frame = av_frame_clone(frame);
    f->pts = pts;
    f->duration = duration;
    m_audioQueue.enqueue(f);
}

Frame* VideoThread::dequeueVideoFrame()
{
    QMutexLocker locker(&m_videoQueueMutex);
    if (m_videoQueue.isEmpty()) return nullptr;
    Frame *f = m_videoQueue.dequeue();
    m_videoQueueCond.wakeOne();
    return f;
}

Frame* VideoThread::dequeueAudioFrame()
{
    QMutexLocker locker(&m_audioQueueMutex);
    if (m_audioQueue.isEmpty()) return nullptr;
    Frame *f = m_audioQueue.dequeue();
    m_audioQueueCond.wakeOne();
    return f;
}

#ifndef AVCODEXMANAGER_H
#define AVCODEXMANAGER_H
#include "pch.h"
#include <string>
#include <memory>
#include <QAbstractVideoSurface>

class SwsScaleContext
{
public:
    SwsScaleContext()
    {

    }
    void SetSrcResolution(int width, int height)
    {
        srcWidth = width;
        srcHeight = height;
    }

    void SetDstResolution(int width, int height)
    {
        dstWidth = width;
        dstHeight = height;
    }
    void SetFormat(AVPixelFormat iformat, AVPixelFormat oformat)
    {
        this->iformat = iformat;
        this->oformat = oformat;
    }
public:
    int srcWidth;
    int srcHeight;
    int dstWidth;
    int dstHeight;
    AVPixelFormat iformat;
    AVPixelFormat oformat;
};


class AvCodexManager:public QAbstractVideoSurface
{
    Q_OBJECT
 public:
    static AvCodexManager *m_instance;
    static AvCodexManager *getInstance()
    {
        return m_instance;
    }
    static int interrupt_cb(void *ctx)
    {
        int  timeout  = 3;
        if(av_gettime() - getInstance()->lastReadPacktTime > timeout *1000 *1000)
        {
            return -1;
        }
        return 0;
    }

    std::shared_ptr<AVPacket> ReadPacketFromSource();
    void Init();
    int OpenOutput(const std::string &outUrl, AVCodecContext *encodeCodec);
    int WritePacket(std::shared_ptr<AVPacket> packet);
    int InitDecodeContext(AVStream *inputStream);
    int initEncoderCodec(AVStream* inputStream,AVCodecContext **encodeContext);
    bool Decode(AVStream* inputStream,AVPacket* packet, AVFrame *frame);
    std::shared_ptr<AVPacket> Encode(AVCodecContext *encodeContext,AVFrame * frame);
    int initSwsContext(struct SwsContext** pSwsContext, SwsScaleContext *swsScaleContext);
    int initSwsFrame(AVFrame *pSwsFrame, int iWidth, int iHeight);

    void run();
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
            QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle) const override;
    void setVideoFrame(const QVideoFrame &frame);
private:
    AvCodexManager();
    AVFormatContext *inputContext = nullptr;
    AVCodecContext *encodeContext = nullptr;
    AVFormatContext * outputContext;
    int64_t lastReadPacktTime ;
    int64_t packetCount = 0;
    struct SwsContext* pSwsContext = nullptr;
    uint8_t * pSwpBuffer = nullptr;

public Q_SLOTS:
    bool present(const QVideoFrame &frame) override;
    void record(int seconds);
    void stop();
    int OpenInput(const std::string &inputUrl);
    int OpenOutput(const std::string &outUrl);
    void CloseInput();
    void CloseOutput();
private:
//    void setRGB24Image(const uint8_t *imgBuf, QSize size);
//    void setRGB32Image(const uint8_t *imgBuf, QSize size);
//    void setMono8Image(const uint8_t *imgBuf, QSize size);
//    void setYUY2Image(const uint8_t *imgBuf, QSize size);
//    void setVYUYImage(const uint8_t *imgBuf, QSize size);
//    void setUYVYImage(const uint8_t *imgBuf, QSize size);
    QImage m_image;
Q_SIGNALS:
};

#endif // AVCODEXMANAGER_H

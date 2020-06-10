#ifndef AVCODEXMANAGER_H
#define AVCODEXMANAGER_H
#include "pch.h"
#include <string>
#include <memory>

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


class AvCodexManager
{
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
    int OpenInput(const std::string &inputUrl);
    std::shared_ptr<AVPacket> ReadPacketFromSource();
    int OpenOutput(const std::string &outUrl, AVCodecContext *encodeCodec);
    void Init();
    void CloseInput();
    void CloseOutput();
    int WritePacket(std::shared_ptr<AVPacket> packet);
    int InitDecodeContext(AVStream *inputStream);
    int initEncoderCodec(AVStream* inputStream,AVCodecContext **encodeContext);
    bool Decode(AVStream* inputStream,AVPacket* packet, AVFrame *frame);
    std::shared_ptr<AVPacket> Encode(AVCodecContext *encodeContext,AVFrame * frame);
    int initSwsContext(struct SwsContext** pSwsContext, SwsScaleContext *swsScaleContext);
    int initSwsFrame(AVFrame *pSwsFrame, int iWidth, int iHeight);
 private:
    AvCodexManager();
    AVFormatContext *inputContext = nullptr;
    AVCodecContext *encodeContext = nullptr;
    AVFormatContext * outputContext;
    int64_t lastReadPacktTime ;
    int64_t packetCount = 0;
    struct SwsContext* pSwsContext = nullptr;
    uint8_t * pSwpBuffer = nullptr;
};

#endif // AVCODEXMANAGER_H

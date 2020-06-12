#ifndef AVCODEXMANAGER_H
#define AVCODEXMANAGER_H
#include "pch.h"
#include <string>
#include <memory>
#include <QAbstractVideoSurface>
#include <QTime>

class MyEncoder
{
public:
    MyEncoder();
public:
    AVFrame *m_pRGBFrame;   //帧对象
    AVFrame *m_pYUVFrame;   //帧对象
    AVCodec *pCodecH264;    //编码器
    AVCodecContext *c;      //编码器数据结构对象
    uint8_t *yuv_buff;      //yuv图像数据区
    uint8_t *rgb_buff;      //rgb图像数据区
    SwsContext *scxt;       //图像格式转换对象
    uint8_t *outbuf;        //编码出来视频数据缓存
    int outbuf_size;        //编码输出数据去大小
    int nDataLen;           //rgb图像数据区长度
    int width;              //输出视频宽度
    int height;             //输出视频高度
    AVPacket pkt;            //数据包结构体
public:
    void Ffmpeg_Encoder_Init();//初始化
    void Ffmpeg_Encoder_Setpara(AVCodecID mycodeid, int vwidth, int vheight);//设置参数,第一个参数为编码器,第二个参数为压缩出来的视频的宽度，第三个视频则为其高度
    void Ffmpeg_Encoder_Encode(FILE *file, uint8_t *data);//编码并写入数据到文件
    void Ffmpeg_Encoder_Close();//关闭
};

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
    void record(int seconds);
    void stop();

public Q_SLOTS:
    bool present(const QVideoFrame &frame) override;
    void RecordButtonEvent(int seconds);
Q_SIGNALS:
    void noticeGLTextureUpdate(const QImage &image);
private:
    QImage m_image;
    QTime m_RecordBeginTime;
    bool m_InRecord;
    MyEncoder myencoder;
    FILE *m_f;
};


#endif // AVCODEXMANAGER_H

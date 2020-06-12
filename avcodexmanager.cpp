#include "avcodexmanager.h"

AvCodexManager *AvCodexManager::m_instance = new AvCodexManager;
#include <iostream>
#include <thread>
#include <QTime>

using namespace std;

AVFrame* convertQImageToAVFrame(QImage& image) {
    //CAUTION: This code is VERY slow. Improve if possible.
    const int width=image.width();
    const int height=image.height();

    auto frame=av_frame_alloc();
    frame->width=width;
    frame->height=height;
    frame->format=AV_PIX_FMT_RGB24;

    int size=avpicture_get_size(AV_PIX_FMT_RGB24,width,height);
    uint8_t* data=(uint8_t*)av_malloc(size);
    for(int i=0; i<height; i++) {
        for(int k=0; k<width; k++) {
            auto pixel=image.pixel(k,i);
            data[i*3*width+3*k]=qRed(pixel);
            data[i*3*width+3*k+1]=qGreen(pixel);
            data[i*3*width+3*k+2]=qBlue(pixel);
        }
    }
    avpicture_fill((AVPicture*)frame,data,AV_PIX_FMT_RGB24,width,height);

    return frame;
}


AvCodexManager::AvCodexManager():
m_InRecord(false),
m_RecordBeginTime(QTime::currentTime())
{}

int AvCodexManager::OpenInput(const std::string &inputUrl) {
    inputContext = avformat_alloc_context();
    lastReadPacktTime = av_gettime();
    inputContext->interrupt_callback.callback = interrupt_cb;
    AVInputFormat *ifmt = av_find_input_format("dshow");
    AVDictionary *format_opts =  nullptr;
    av_dict_set_int(&format_opts, "rtbufsize", 18432000  , 0);

    int ret = avformat_open_input(&inputContext, inputUrl.c_str(), ifmt,&format_opts);
    if(ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Input file open input failed\n");
        return  ret;
    }
    ret = avformat_find_stream_info(inputContext,nullptr);
    if(ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Find input file stream inform failed\n");
    }
    else
    {
        av_log(nullptr, AV_LOG_FATAL, "Open input file  %s success\n",inputUrl.c_str());
    }
    return ret;
}

bool AvCodexManager::present(const QVideoFrame &frame)
{
    if(frame.isMapped())
    {
        setVideoFrame(frame);
    }
    else
    {
        QVideoFrame f(frame);
        f.map(QAbstractVideoBuffer::ReadOnly);
        setVideoFrame(f);
    }
    return true;
}

QList<QVideoFrame::PixelFormat> AvCodexManager::supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType)  const
{
    Q_UNUSED(handleType);
    QList<QVideoFrame::PixelFormat> list;
    list << QVideoFrame::Format_RGB32;
    list << QVideoFrame::Format_ARGB32;
    list << QVideoFrame::Format_RGB24;
    list << QVideoFrame::Format_UYVY;
    list << QVideoFrame::Format_Y8;
    list << QVideoFrame::Format_YUYV;
    return list;
}

void AvCodexManager::setVideoFrame(const QVideoFrame &frame)
{
    QImage res(frame.bits(),
               frame.width(),
               frame.height(),
               frame.bytesPerLine(),
               QImage::Format_RGB32);

    emit noticeGLTextureUpdate(res);
    res.convertTo(QImage::Format_RGB888, Qt::ColorOnly);
    myencoder.Ffmpeg_Encoder_Encode(m_f, (uchar*)res.bits());
}

shared_ptr<AVPacket> AvCodexManager::ReadPacketFromSource()
{
    shared_ptr<AVPacket> packet(static_cast<AVPacket*>(av_malloc(sizeof(AVPacket))), [&](AVPacket *p) { av_packet_free(&p); av_freep(&p);});
    av_init_packet(packet.get());
    lastReadPacktTime = av_gettime();
    int ret = av_read_frame(inputContext, packet.get());
    if(ret >= 0)
    {
        return packet;
    }
    else
    {
        return nullptr;
    }
}

void AvCodexManager::run()
{
    SwsScaleContext swsScaleContext;
    AVFrame *videoFrame = av_frame_alloc();
    AVFrame *pSwsVideoFrame = av_frame_alloc();
    int ret = OpenInput("video=USB2.0 HD UVC WebCam");

    if(ret <0)
    {
        CloseInput();
        CloseOutput();

        while(true)
        {
            this_thread::sleep_for(chrono::seconds(100));
        }
        return;
    }

    InitDecodeContext(inputContext->streams[0]);
    ret = initEncoderCodec(inputContext->streams[0],&encodeContext);

    if(ret >= 0)
    {
        ret = OpenOutput("D:\\usbCamera.mp4",encodeContext);
    }
    if(ret <0)
    {
        CloseInput();
        CloseOutput();

        while(true)
        {
            this_thread::sleep_for(chrono::seconds(100));
        }
        return;
    }
    swsScaleContext.SetSrcResolution(inputContext->streams[0]->codec->width, inputContext->streams[0]->codec->height);

    swsScaleContext.SetDstResolution(encodeContext->width,encodeContext->height);
    swsScaleContext.SetFormat(inputContext->streams[0]->codec->pix_fmt, encodeContext->pix_fmt);
    initSwsContext(&pSwsContext, &swsScaleContext);
    initSwsFrame(pSwsVideoFrame,encodeContext->width, encodeContext->height);
    int64_t startTime = av_gettime();
    while(true)
    {
        auto packet = ReadPacketFromSource();
        if(av_gettime() - startTime > 10 * 1000 * 1000)
        {
            break;
        }
        if(packet && packet->stream_index == 0)
        {
            if(Decode(inputContext->streams[0],packet.get(),videoFrame))
            {
                sws_scale(pSwsContext, (const uint8_t *const *)videoFrame->data,
                    videoFrame->linesize, 0, inputContext->streams[0]->codec->height, (uint8_t *const *)pSwsVideoFrame->data, pSwsVideoFrame->linesize);
                auto packetEncode = Encode(encodeContext,pSwsVideoFrame);
                if(packetEncode)
                {
                    ret = WritePacket(packetEncode);
                    //cout <<"ret:" << ret<<endl;
                }
            }
        }
    }
    cout <<"record success"<<endl;
    av_frame_free(&videoFrame);
    avcodec_close(encodeContext);
    av_frame_free(&pSwsVideoFrame);

    CloseInput();
    CloseOutput();

    while(true)
    {
        this_thread::sleep_for(chrono::seconds(100));
    }
    return;
}

int AvCodexManager::OpenOutput(const std::string &outUrl)
{
    if (encodeContext != nullptr) {
        return OpenOutput(outUrl, encodeContext);
    } else {
        return -1;
    }
}

int AvCodexManager::OpenOutput(const string &outUrl,AVCodecContext *encodeCodec)
{

    int ret  = avformat_alloc_output_context2(&outputContext, nullptr, "mp4", outUrl.c_str());
    if(ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "open output context failed\n");
        goto Error;
    }

    ret = avio_open2(&outputContext->pb, outUrl.c_str(), AVIO_FLAG_WRITE,nullptr, nullptr);
    if(ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "open avio failed");
        goto Error;
    }

    for(int i = 0; i < inputContext->nb_streams; i++)
    {
        if(inputContext->streams[i]->codec->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO)
        {
            continue;
        }
        AVStream * stream = avformat_new_stream(outputContext, encodeCodec->codec);
        ret = avcodec_copy_context(stream->codec, encodeCodec);
        if(ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR, "copy coddec context failed");
            goto Error;
        }
    }

    ret = avformat_write_header(outputContext, nullptr);
    if(ret < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "format write header failed");
        goto Error;
    }

    av_log(nullptr, AV_LOG_FATAL, " Open output file success %s\n",outUrl.c_str());
    return ret ;
Error:
    if(outputContext)
    {
        for(int i = 0; i < outputContext->nb_streams; i++)
        {
            avcodec_close(outputContext->streams[i]->codec);
        }
        avformat_close_input(&outputContext);
    }
    return ret ;
}

void AvCodexManager::Init()
{
//    av_register_all();
//    avfilter_register_all();
//    avformat_network_init();
//    avdevice_register_all();
//    av_log_set_level(AV_LOG_ERROR);
    myencoder.Ffmpeg_Encoder_Init();//初始化编码器
    myencoder.Ffmpeg_Encoder_Setpara(AV_CODEC_ID_H264, 1280, 720);//设置编码器参数
    char * filename = "myData.h264";
    fopen_s(&m_f, filename, "wb");//打开文件存储编码完成数据

    std::cout << "AvCodexManager " << __func__ << " success." << std::endl;
}

void AvCodexManager::record(int seconds)
{
    auto cur = QTime::currentTime();
    if (!m_InRecord)
    {
        std::cout << __FUNCTION__
                  << cur.toString("hh:mm:ss.zzz").toStdString()
                  << "  recordstart  "
                  << std::endl;
        m_RecordBeginTime = cur;
        m_InRecord = true;
    }
}
void AvCodexManager::stop()
{
    auto cur = QTime::currentTime();
    if (m_InRecord && ((cur.second()  - m_RecordBeginTime.second()) > 10))
    {
        std::cout << __FUNCTION__
                  << cur.toString("hh:mm:ss.zzz").toStdString()
                  << "   record stop   "
                  << std::endl;
        m_InRecord = false;
    }
}

void AvCodexManager::RecordButtonEvent(int seconds)
{
    if (m_InRecord)
    {
        stop();
    } else
    {
        record(seconds);
    }
}

void AvCodexManager::CloseInput()
{
    if(inputContext != nullptr)
    {
        avformat_close_input(&inputContext);
    }

    if(pSwsContext)
    {
        sws_freeContext(pSwsContext);
    }
}

void AvCodexManager::CloseOutput()
{
    if(outputContext != nullptr)
    {
        int ret = av_write_trailer(outputContext);
        avformat_close_input(&outputContext);
    }
}

int AvCodexManager::WritePacket(shared_ptr<AVPacket> packet)
{
    auto inputStream = inputContext->streams[packet->stream_index];
    auto outputStream = outputContext->streams[packet->stream_index];
    packet->pts = packet->dts = packetCount * (outputContext->streams[0]->time_base.den) /
                     outputContext->streams[0]->time_base.num / 30 ;
    packetCount++;
    return av_interleaved_write_frame(outputContext, packet.get());
}

int AvCodexManager::InitDecodeContext(AVStream *inputStream)
{
    auto codecId = inputStream->codec->codec_id;
    auto codec = avcodec_find_decoder(codecId);
    if (!codec)
    {
        return -1;
    }

    int ret = avcodec_open2(inputStream->codec, codec, NULL);
    return ret;

}

int AvCodexManager::initEncoderCodec(AVStream* inputStream, AVCodecContext **encodeContext)
    {
        AVCodec *  picCodec;

        picCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
        (*encodeContext) = avcodec_alloc_context3(picCodec);

        (*encodeContext)->codec_id = picCodec->id;
        (*encodeContext)->has_b_frames = 0;
        (*encodeContext)->time_base.num = inputStream->codec->time_base.num;
        (*encodeContext)->time_base.den = inputStream->codec->time_base.den;
        (*encodeContext)->pix_fmt =  *picCodec->pix_fmts;
        (*encodeContext)->width = inputStream->codec->width;
        (*encodeContext)->height =inputStream->codec->height;
        (*encodeContext)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        int ret = avcodec_open2((*encodeContext), picCodec, nullptr);
        if (ret < 0)
        {
            std::cout<<"open video codec failed"<<endl;
            return  ret;
        }
            return 1;
    }

bool AvCodexManager::Decode(AVStream* inputStream,AVPacket* packet, AVFrame *frame)
{
    int gotFrame = 0;
    auto hr = avcodec_decode_video2(inputStream->codec, frame, &gotFrame, packet);
    if (hr >= 0 && gotFrame != 0)
    {
        return true;
    }
    return false;
}


std::shared_ptr<AVPacket> AvCodexManager::Encode(AVCodecContext *encodeContext,AVFrame * frame)
{
    int gotOutput = 0;
    std::shared_ptr<AVPacket> pkt(static_cast<AVPacket*>(av_malloc(sizeof(AVPacket))), [&](AVPacket *p) { av_packet_free(&p); av_freep(&p); });
    av_init_packet(pkt.get());
    pkt->data = NULL;
    pkt->size = 0;
    int ret = avcodec_encode_video2(encodeContext, pkt.get(), frame, &gotOutput);
    if (ret >= 0 && gotOutput)
    {
        return pkt;
    }
    else
    {
        return nullptr;
    }
}


int AvCodexManager::initSwsContext(struct SwsContext** pSwsContext, SwsScaleContext *swsScaleContext)
{
    *pSwsContext = sws_getContext(swsScaleContext->srcWidth, swsScaleContext->srcHeight, swsScaleContext->iformat,
        swsScaleContext->dstWidth, swsScaleContext->dstHeight, swsScaleContext->oformat,
        SWS_BICUBIC,
        NULL, NULL, NULL);
    if (pSwsContext == NULL)
    {
        return -1;
    }
    return 0;
}

int AvCodexManager::initSwsFrame(AVFrame *pSwsFrame, int iWidth, int iHeight)
{
    int numBytes = av_image_get_buffer_size(encodeContext->pix_fmt, iWidth, iHeight, 1);
    /*if(pSwpBuffer)
    {
        av_free(pSwpBuffer);
    }*/
    pSwpBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pSwsFrame->data, pSwsFrame->linesize, pSwpBuffer, encodeContext->pix_fmt, iWidth, iHeight, 1);
    pSwsFrame->width = iWidth;
    pSwsFrame->height = iHeight;
    pSwsFrame->format = encodeContext->pix_fmt;
    return 1;
}

MyEncoder::MyEncoder()
{
}

void MyEncoder::Ffmpeg_Encoder_Init()
{
    av_register_all();
    avcodec_register_all();

    m_pRGBFrame = new AVFrame[1];//RGB帧数据赋值
    m_pYUVFrame = new AVFrame[1];//YUV帧数据赋值

    c = NULL;//解码器指针对象赋初值
}

void MyEncoder::Ffmpeg_Encoder_Setpara(AVCodecID mycodeid, int vwidth, int vheight)
{
    pCodecH264 = avcodec_find_encoder(mycodeid);//查找h264编码器
    if (!pCodecH264)
    {
        fprintf(stderr, "h264 codec not found\n");
        exit(1);
    }
    width = vwidth;
    height = vheight;

    c = avcodec_alloc_context3(pCodecH264);//函数用于分配一个AVCodecContext并设置默认值，如果失败返回NULL，并可用av_free()进行释放
    c->bit_rate = 400000; //设置采样参数，即比特率
    c->width = vwidth;//设置编码视频宽度
    c->height = vheight; //设置编码视频高度
    c->time_base.den = 30;//设置帧率,num为分子和den为分母，如果是1/25则表示25帧/s
    c->time_base.num = 1;
    c->gop_size = 10; //设置GOP大小,该值表示每10帧会插入一个I帧
    c->max_b_frames = 1;//设置B帧最大数,该值表示在两个非B帧之间，所允许插入的B帧的最大帧数
    c->pix_fmt = AV_PIX_FMT_YUV420P;//设置像素格式

    av_opt_set(c->priv_data, "tune", "zerolatency", 0);//设置编码器的延时，解决前面的几十帧不出数据的情况

    if (avcodec_open2(c, pCodecH264, NULL) < 0)return;//打开编码器

    nDataLen = vwidth*vheight * 3;//计算图像rgb数据区长度

    yuv_buff = new uint8_t[nDataLen / 2];//初始化数据区，为yuv图像帧准备填充缓存
    rgb_buff = new uint8_t[nDataLen];//初始化数据区，为rgb图像帧准备填充缓存
    outbuf_size = 100000;////初始化编码输出数据区
    outbuf = new uint8_t[outbuf_size];

    scxt = sws_getContext(c->width, c->height, AV_PIX_FMT_BGRA, c->width, c->height, AV_PIX_FMT_YUV420P, SWS_POINT, NULL, NULL, NULL);//初始化格式转换函数
}

void MyEncoder::Ffmpeg_Encoder_Encode(FILE *file, uint8_t *data)
{
    av_init_packet(&pkt);
    memcpy(rgb_buff, data, nDataLen);//拷贝图像数据到rgb图像帧缓存中准备处理
    avpicture_fill((AVPicture*)m_pRGBFrame, (uint8_t*)rgb_buff, AV_PIX_FMT_RGB24, width, height);//将rgb_buff填充到m_pRGBFrame
    //av_image_fill_arrays((AVPicture*)m_pRGBFrame, (uint8_t*)rgb_buff, AV_PIX_FMT_RGB24, width, height);
    avpicture_fill((AVPicture*)m_pYUVFrame, (uint8_t*)yuv_buff, AV_PIX_FMT_YUV420P, width, height);//将yuv_buff填充到m_pYUVFrame
    sws_scale(scxt, m_pRGBFrame->data, m_pRGBFrame->linesize, 0, c->height, m_pYUVFrame->data, m_pYUVFrame->linesize);// 将RGB转化为YUV
    int myoutputlen = 0;
    int returnvalue = avcodec_encode_video2(c, &pkt, m_pYUVFrame, &myoutputlen);
    if (returnvalue == 0)
    {
        fwrite(pkt.data, 1, pkt.size, file);
    }
    av_free_packet(&pkt);
}

void MyEncoder::Ffmpeg_Encoder_Close()
{
    delete[]m_pRGBFrame;
    delete[]m_pYUVFrame;
    delete[]rgb_buff;
    delete[]yuv_buff;
    delete[]outbuf;
    sws_freeContext(scxt);
    avcodec_close(c);//关闭编码器
    av_free(c);
}


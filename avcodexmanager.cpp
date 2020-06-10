#include "avcodexmanager.h"

AvCodexManager *AvCodexManager::m_instance = new AvCodexManager;
#include <iostream>
#include <thread>

using namespace std;

AvCodexManager::AvCodexManager() {}

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
    std::cout << __FUNCTION__ << std::endl;
    // 处理捕获的帧
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
    switch (frame.pixelFormat())
    {
    case QVideoFrame::Format_RGB24:
    case QVideoFrame::Format_RGB32:
    case QVideoFrame::Format_ARGB32:
    case QVideoFrame::Format_ARGB32_Premultiplied:
    case QVideoFrame::Format_YUYV:
    case QVideoFrame::Format_UYVY:
    case QVideoFrame::Format_Y8:
    default:
        break;
    }
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
    Init();
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
    av_register_all();
    avfilter_register_all();
    avformat_network_init();
    avdevice_register_all();
    av_log_set_level(AV_LOG_ERROR);
    std::cout << "AvCodexManager " << __func__ << " success." << std::endl;
}

void AvCodexManager::record(int seconds)
{
    //todo
}
void AvCodexManager::stop()
{
    //todo
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
    //cout <<"pts:"<<packet->pts<<endl;
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

//int _tmain(int argc, _TCHAR* argv[])
//{
//    SwsScaleContext swsScaleContext;
//    AVFrame *videoFrame = av_frame_alloc();
//    AVFrame *pSwsVideoFrame = av_frame_alloc();

//    int ret = OpenInput("video=USB2.0 HD UVC WebCam");

//    if(ret <0) goto Error;

//    InitDecodeContext(inputContext->streams[0]);


//    ret = initEncoderCodec(inputContext->streams[0],&encodeContext);

//    if(ret >= 0)
//    {
//        ret = OpenOutput("D:\\usbCamera.mp4",encodeContext);
//    }
//    if(ret <0) goto Error;

//    swsScaleContext.SetSrcResolution(inputContext->streams[0]->codec->width, inputContext->streams[0]->codec->height);

//    swsScaleContext.SetDstResolution(encodeContext->width,encodeContext->height);
//    swsScaleContext.SetFormat(inputContext->streams[0]->codec->pix_fmt, encodeContext->pix_fmt);
//    initSwsContext(&pSwsContext, &swsScaleContext);
//    initSwsFrame(pSwsVideoFrame,encodeContext->width, encodeContext->height);
//    int64_t startTime = av_gettime();
//     while(true)
//     {
//        auto packet = ReadPacketFromSource();
//        if(av_gettime() - startTime > 10 * 1000 * 1000)
//        {
//            break;
//        }
//        if(packet && packet->stream_index == 0)
//        {
//            if(Decode(inputContext->streams[0],packet.get(),videoFrame))
//            {
//                sws_scale(pSwsContext, (const uint8_t *const *)videoFrame->data,
//                    videoFrame->linesize, 0, inputContext->streams[0]->codec->height, (uint8_t *const *)pSwsVideoFrame->data, pSwsVideoFrame->linesize);
//                auto packetEncode = Encode(encodeContext,pSwsVideoFrame);
//                if(packetEncode)
//                {
//                    ret = WritePacket(packetEncode);
//                    //cout <<"ret:" << ret<<endl;
//                }

//            }

//        }

//     }
//     cout <<"Get Picture End "<<endl;
//     av_frame_free(&videoFrame);
//     avcodec_close(encodeContext);
//     av_frame_free(&pSwsVideoFrame);

//    Error:
//    CloseInput();
//    CloseOutput();

//    while(true)
//    {
//        this_thread::sleep_for(chrono::seconds(100));
//    }
//    return 0;
//}


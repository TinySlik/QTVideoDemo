#ifndef AVCODEXMANAGER_H
#define AVCODEXMANAGER_H

extern "C"
{
    #include "libavutil/opt.h"
    #include "libavutil/channel_layout.h"
    #include "libavutil/common.h"
    #include "libavutil/imgutils.h"
    #include "libavutil/mathematics.h"
    #include "libavutil/samplefmt.h"
    #include "libavutil/time.h"
    #include "libavutil/fifo.h"
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavformat/avio.h"
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersink.h"
    #include "libavfilter/buffersrc.h"
    #include "libswscale/swscale.h"
    #include "libswresample/swresample.h"
    #include "libavdevice/avdevice.h"
}

class AvCodexManager
{
    static AvCodexManager *m_instance;
    static AvCodexManager *getInstance()
    {
        return m_instance;
    }
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

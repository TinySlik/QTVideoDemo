#include "avcodexmanager.h"

AvCodexManager *AvCodexManager::m_instance = new AvCodexManager;

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

AvCodexManager::AvCodexManager()
{

}

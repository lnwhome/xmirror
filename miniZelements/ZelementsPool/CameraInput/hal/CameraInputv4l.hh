#ifndef CAMERAINPUTV4L_HH
#define CAMERAINPUTV4L_HH

#include <hal/ZelementsPool/CameraInput/CameraInput.hh>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>

namespace mfw { namespace zel {

/**
  * class CamerInputv4l implements CameraInput on linux using V4L2
  * 
  */
class CameraInputv4l : public CameraInput {
public:

    CameraInputv4l ( );

    virtual ~CameraInputv4l ( );

    bool Querry(std::vector<CameraInput::CameraInputDscr> &v);
private:
    static const int MY_MMAP_BUFFERS = 3;
    char * m_MmapedPtrs[MY_MMAP_BUFFERS];
    v4l2_buffer m_CapBuf[MY_MMAP_BUFFERS];
    v4l2_requestbuffers m_ReqBuf;
    int m_CameraFd;
    unsigned int m_CurPicSize;

    int GetMmapBuffers();
    void OnCmd(evt::Event *a_Evt);
    void OnState(State a_State);    
    void OnIdle();

};

};};

#endif // CAMERAINPUTV4L_HH

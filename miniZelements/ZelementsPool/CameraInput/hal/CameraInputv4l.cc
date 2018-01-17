#include "zinc/debug.hh"
#include "CameraInputv4l.hh"
#include <iostream>
#include <typeinfo>
#include <pthread.h>

namespace mfw { namespace zel {

CameraInput* CreateCameraInput() {
    return new CameraInputv4l();
}

CameraInputv4l::CameraInputv4l ( ) : m_CameraFd(-1) {
    for (int i=0;i<MY_MMAP_BUFFERS; ++i) {
        m_MmapedPtrs[i] = NULL;
    }
    m_Name = std::string("CameraInputv4l");
}

CameraInputv4l::~CameraInputv4l ( ) {
    Kick(new StateEvt(ZERO));
    Wait(this);
}

bool CameraInputv4l::Querry(std::vector<CameraInput::CameraInputDscr> &v)
{
    SmartLock mylock(this);

    int fd = (m_CameraFd == -1) ? open(m_URI.c_str(), O_RDWR) : m_CameraFd;
    int v4l2_inx = 0;
    std::list<CameraInputDscr> list_d1, list_d2;
    std::vector<CameraInputDscr> vector_d3;
    v4l2_capability camera_cap;
    std::string busid;
    
    if (ioctl(fd, VIDIOC_QUERYCAP, &camera_cap) >= 0) {
        busid = std::string(reinterpret_cast<const char*>(&camera_cap.bus_info[0]));
    }

    //pixel formats
    v4l2_fmtdesc camera_fmt;
    memset(&camera_fmt, 0, sizeof(struct v4l2_fmtdesc));
    camera_fmt.index = 0;
    camera_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_inx = 0;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &camera_fmt) != -1) {
        LOG_TRACE(("enum %i: %c%c%c%c (%s)\n", v4l2_inx,
                camera_fmt.pixelformat >> 0, camera_fmt.pixelformat >> 8,
                camera_fmt.pixelformat >> 16, camera_fmt.pixelformat >> 24, camera_fmt.description));
	    
        CameraInputDscr dscr;
        dscr.busid = busid;
        switch (camera_fmt.pixelformat) {
            case YUYV:
                dscr.fcc = YUYV;
                break;
            case YUY2:
                dscr.fcc = YUY2;
                break;
            case YVYU:
                dscr.fcc = YVYU;
                break;
            case YV12:
                dscr.fcc = YV12;
                break;
            case BGRA:
                dscr.fcc = BGRA;
                break;
            default:
                dscr.fcc = AUTO;
                break;
        }
        list_d1.push_back(dscr);
        memset(&camera_fmt, 0, sizeof(struct v4l2_fmtdesc));
        camera_fmt.index = ++v4l2_inx;
        camera_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    //dimention
    for (std::list<CameraInputDscr>::iterator i = list_d1.begin(); i != list_d1.end(); ++i) {
        v4l2_frmsizeenum camera_dim;
        memset(&camera_dim, 0, sizeof(struct v4l2_frmsizeenum));
        camera_dim.index = 0;
        camera_dim.pixel_format = i->fcc;
        v4l2_inx = 0;
        while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &camera_dim) != -1) {
	    switch (camera_dim.type) {
                case V4L2_FRMSIZE_TYPE_DISCRETE:
                    LOG_TRACE(("enumD :%d, %u %u", v4l2_inx, camera_dim.discrete.width, camera_dim.discrete.height));
                    i->width = camera_dim.discrete.width;
                    i->height = camera_dim.discrete.height;
                    list_d2.push_back(*i);
                    break;
                case V4L2_FRMSIZE_TYPE_STEPWISE:
                    LOG_TRACE(("enumS :%d, %u:%u - %u x %u:%u - %u",
                                  v4l2_inx,
                                  camera_dim.stepwise.min_width,
                                  camera_dim.stepwise.max_width,
                                  camera_dim.stepwise.step_width,
                                  camera_dim.stepwise.min_height,
                                  camera_dim.stepwise.max_height,
                                  camera_dim.stepwise.step_height
                                  ));
                    //TODO:add resonable number of resolutions in between min and max
                    i->width = camera_dim.stepwise.min_width;
                    i->height = camera_dim.stepwise.min_height;
                    list_d2.push_back(*i);
                    i->width = camera_dim.stepwise.max_width;
                    i->height = camera_dim.stepwise.max_height;
                    list_d2.push_back(*i);
                    break;
                case V4L2_FRMSIZE_TYPE_CONTINUOUS:
                    LOG_TRACE(("enumC :%d, %u:%u - %u x %u:%u - %u",
                                  v4l2_inx,
                                  camera_dim.stepwise.min_width,
                                  camera_dim.stepwise.max_width,
                                  1,
                                  camera_dim.stepwise.min_height,
                                  camera_dim.stepwise.max_height,
                                  1
                                  ));
                    //TODO:add resonable number of resolutions in between min and max
                    i->width = camera_dim.stepwise.min_width;
                    i->height = camera_dim.stepwise.min_height;
                    list_d2.push_back(*i);
                    i->width = camera_dim.stepwise.max_width;
                    i->height = camera_dim.stepwise.max_height;
                    list_d2.push_back(*i);
                    break;
            }
            memset(&camera_dim, 0, sizeof(struct v4l2_frmsizeenum));
            camera_dim.index = ++v4l2_inx;
            camera_dim.pixel_format = i->fcc;
        }
    }
    //frame rate 
    for (std::list<CameraInputDscr>::iterator i = list_d2.begin(); i != list_d2.end(); ++i) {
        v4l2_frmivalenum camera_fr;
        memset(&camera_fr, 0, sizeof(struct v4l2_frmivalenum));
        camera_fr.index = 0;
        camera_fr.pixel_format = i->fcc;
        camera_fr.width = i->width;
        camera_fr.height = i->height;
        v4l2_inx = 0;
        while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &camera_fr) != -1) {
	    switch (camera_fr.type) {
                case V4L2_FRMIVAL_TYPE_DISCRETE:
                    LOG_TRACE(("enumD :%d, %u/%u", v4l2_inx, camera_fr.discrete.numerator, camera_fr.discrete.denominator));
                    i->fps = camera_fr.discrete.denominator / camera_fr.discrete.numerator;
                    vector_d3.push_back(*i);
                    break;
                case V4L2_FRMIVAL_TYPE_STEPWISE:
                    LOG_TRACE(("enumS :%d, %u/%u:%u/%u - %u/%u",
                                  v4l2_inx,
                                  camera_fr.stepwise.min.numerator,camera_fr.stepwise.min.denominator,
                                  camera_fr.stepwise.max.numerator,camera_fr.stepwise.max.denominator,
                                  camera_fr.stepwise.step.numerator,camera_fr.stepwise.step.denominator
                                  ));
                    //TODO:add resonable number of fps in between min and max
                    i->fps = camera_fr.stepwise.min.denominator / camera_fr.stepwise.min.numerator;
                    vector_d3.push_back(*i);
                    i->fps = camera_fr.stepwise.max.denominator / camera_fr.stepwise.max.numerator;
                    vector_d3.push_back(*i);
                    break;
                case V4L2_FRMIVAL_TYPE_CONTINUOUS:
                    LOG_TRACE(("enumC :%C, %u/%u:%u/%u - %u/%u",
                                  v4l2_inx,
                                  camera_fr.stepwise.min.numerator,camera_fr.stepwise.min.denominator,
                                  camera_fr.stepwise.max.numerator,camera_fr.stepwise.max.denominator,
                                  camera_fr.stepwise.step.numerator,camera_fr.stepwise.step.denominator
                                  ));
                    //TODO:add resonable number of fps in between min and max
                    i->fps = camera_fr.stepwise.min.denominator / camera_fr.stepwise.min.numerator;
                    vector_d3.push_back(*i);
                    i->fps = camera_fr.stepwise.max.denominator / camera_fr.stepwise.max.numerator;
                    vector_d3.push_back(*i);
                    break;
            }
            memset(&camera_fr, 0, sizeof(struct v4l2_frmivalenum));
            camera_fr.index = ++v4l2_inx;
            camera_fr.pixel_format = i->fcc;
            camera_fr.width = i->width;
            camera_fr.height = i->height;
        }
    }

    if (m_CameraFd == -1 && fd != -1) {
        ::close(fd);
    }
    v = vector_d3;

    return !v.empty();
}

void CameraInputv4l::OnCmd(evt::Event *a_Evt) {
    (void)a_Evt;
}

void CameraInputv4l::OnState(State a_State) {
    SmartLock mylock(this);

    if (m_State == a_State)  // no-op if already in state
        return;

    LOG_MESSAGE(("Changing state from %s to %s...", StateName(m_State), StateName(a_State) ));

    if (m_State == PAUSED && a_State == PLAYING) {
    }

    if (m_State == ZERO && a_State == READY) {
        v4l2_capability camera_cap;
        
        if ((m_CameraFd = open(m_URI.c_str(), O_RDWR)) < 0) {
    	    PostEvent(new ErrEvt("noo dev", CAMERA_INPUT_ERR_NO_DEV, this));
            return;
        }

        if (ioctl(m_CameraFd, VIDIOC_QUERYCAP, &camera_cap) < 0) {
    	    ::close(m_CameraFd);
            PostEvent(new ErrEvt("cap querry failed", CAMERA_INPUT_ERR_NO_CAP, this));
            return;
        }
        if ((camera_cap.capabilities & V4L2_CAP_STREAMING) == 0) {
    	    ::close(m_CameraFd);
    	    PostEvent(new ErrEvt("no streaming cap", CAMERA_INPUT_ERR_NO_CAP_STREAMING, this));
            return;
        }
        
        v4l2_input camera_input;
        memset(&camera_input, 0, sizeof (camera_input));
        camera_input.index = 0;
        if (ioctl(m_CameraFd, VIDIOC_ENUMINPUT, &camera_input) == -1) {
    	    ::close(m_CameraFd);
    	    PostEvent(new ErrEvt("no input", CAMERA_INPUT_ERR_NO_INPUT, this));
            return ;
        }
        LOG_TRACE(("%s: name = \"%s\", type 0x%08X, status %08x\n", __FUNCTION__, camera_input.name, camera_input.type, camera_input.status));
        if (ioctl(m_CameraFd, VIDIOC_S_INPUT, &camera_input) == -1) {
    	    ::close(m_CameraFd);
    	    PostEvent(new ErrEvt("no input", CAMERA_INPUT_ERR_NO_INPUT, this));
            return ;
        }
    
    
        v4l2_fmtdesc camera_fmt;
        memset(&camera_fmt, 0, sizeof(struct v4l2_fmtdesc));
        camera_fmt.index = 0;
        camera_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        int v4l2_inx = 0;
        while (ioctl(m_CameraFd, VIDIOC_ENUM_FMT, &camera_fmt) != -1) {
            LOG_MESSAGE(("%i: %c%c%c%c (%s)\n", v4l2_inx,
                   camera_fmt.pixelformat >> 0, camera_fmt.pixelformat >> 8,
                   camera_fmt.pixelformat >> 16, camera_fmt.pixelformat >> 24, camera_fmt.description));
	        
            if (camera_fmt.pixelformat == m_Dscr.fcc) {
	            break;
            }

            memset(&camera_fmt, 0, sizeof(struct v4l2_fmtdesc));
            camera_fmt.index = ++v4l2_inx;
            camera_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        }
        
        if (camera_fmt.pixelformat != m_Dscr.fcc) {
    	    ::close(m_CameraFd);
            PostEvent(new ErrEvt("no format avail", CAMERA_INPUT_ERR_FCC, this));
	        return;
        }
   

        v4l2_format camera_ffmt;
        memset(&camera_ffmt, 0, sizeof(struct v4l2_format));
        camera_ffmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        camera_ffmt.fmt.pix.field = V4L2_FIELD_ANY;
        camera_ffmt.fmt.pix.width = m_Dscr.width;
        camera_ffmt.fmt.pix.height = m_Dscr.height;
        camera_ffmt.fmt.pix.pixelformat = m_Dscr.fcc;
        if (ioctl(m_CameraFd, VIDIOC_TRY_FMT, &camera_ffmt) == -1 || camera_ffmt.fmt.pix.pixelformat != camera_fmt.pixelformat) {
    	    ::close(m_CameraFd);
            PostEvent(new ErrEvt("wrong pic size", CAMERA_INPUT_ERR_SIZE, this));
	        return ;
        }
        
        m_CurPicSize = camera_ffmt.fmt.pix.sizeimage;

        if (ioctl(m_CameraFd, VIDIOC_S_FMT, &camera_ffmt) == -1) {
    	    ::close(m_CameraFd);
            PostEvent(new ErrEvt("wrong pic size", CAMERA_INPUT_ERR_SIZE, this));
	        return ;
        }


        v4l2_streamparm fps;
        memset(&fps, 0, sizeof(fps));
        fps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(m_CameraFd, VIDIOC_G_PARM, &fps) < 0) {
            LOG_WARNING(("no fps info"));
        }
        fps.parm.capture.timeperframe.numerator = 1;
        fps.parm.capture.timeperframe.denominator = m_Dscr.fps;
        if (ioctl(m_CameraFd, VIDIOC_S_PARM, &fps) < 0) {
            PostEvent(new ErrEvt("failed to set fps", CAMERA_INPUT_ERR_FPS, this));
            return ;
        }
        //check what we got
        memset(&fps, 0, sizeof(fps));
        fps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(m_CameraFd, VIDIOC_G_PARM, &fps) < 0) {
            LOG_WARNING(("no fps info"));
        };
        LOG_MESSAGE(("FPS:%d\n", fps.parm.capture.timeperframe.denominator / fps.parm.capture.timeperframe.numerator));
        
        if (GetMmapBuffers() < 0) {
    	    ::close(m_CameraFd);
            for (int i=0;i<MY_MMAP_BUFFERS;++i) {
                if (m_MmapedPtrs[i] != NULL) {
                    ::munmap(m_MmapedPtrs[i], m_CapBuf[i].length);
                    m_MmapedPtrs[i] = NULL;
                }
            }
            PostEvent(new ErrEvt("mmap failed", CAMERA_INPUT_ERR_MMAP, this));
	        return ;
        }

        for (size_t i = 0; i < m_ReqBuf.count; ++i) {
            v4l2_buffer buf;

	        memset(&buf, 0, sizeof(struct v4l2_buffer));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (ioctl(m_CameraFd, VIDIOC_QBUF, &buf) == -1) {
    	        ::close(m_CameraFd);
                PostEvent(new ErrEvt("buf queue failed", CAMERA_INPUT_ERR_QUEUE, this));
                //TODO:UNMAP!!!
                return ;
            }
        }
    }
    
    if (m_State == READY && a_State == PLAYING) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(m_CameraFd, VIDIOC_STREAMON, &type) == -1) {
            PostEvent(new ErrEvt("stream start failed", CAMERA_INPUT_ERR_STREAM, this));
            return ;
        }
    }
    
    if (m_State == PLAYING && a_State == PAUSED) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(m_CameraFd, VIDIOC_STREAMOFF, &type) == -1) {
            PostEvent(new ErrEvt("stream stop failed", CAMERA_INPUT_ERR_STREAM, this));
            return ;
        }
    }

    if (a_State == ZERO) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(m_CameraFd, VIDIOC_STREAMOFF, &type) == -1) {
            PostEvent(new ErrEvt("stream stop failed", CAMERA_INPUT_ERR_STREAM, this));
            return ;
        }
        for (int i=0;i<MY_MMAP_BUFFERS;++i) {
            if (m_MmapedPtrs[i] != NULL) {
                ::munmap(m_MmapedPtrs[i], m_CapBuf[i].length);
                m_MmapedPtrs[i] = NULL;
            }
        }
        ::close(m_CameraFd);
        m_CameraFd = -1;
        //TODO:unmap
    }

    m_State = a_State;
};

int CameraInputv4l::GetMmapBuffers() {
    size_t i;

    memset(&m_ReqBuf, 0, sizeof(struct v4l2_requestbuffers));
    m_ReqBuf.count = MY_MMAP_BUFFERS;
    m_ReqBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m_ReqBuf.memory = V4L2_MEMORY_MMAP;
    if ((ioctl(m_CameraFd, VIDIOC_REQBUFS, &m_ReqBuf) == -1) || (m_ReqBuf.count < 2)) {
        return -1;
    }

    for (i=0;i<m_ReqBuf.count;++i) {
        memset(&m_CapBuf[i], 0, sizeof(struct v4l2_buffer));
        m_CapBuf[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        m_CapBuf[i].memory = V4L2_MEMORY_MMAP;
        m_CapBuf[i].index = i;
        if (ioctl(m_CameraFd, VIDIOC_QUERYBUF, &m_CapBuf[i]) == -1) {
            return -1;
        }

        m_MmapedPtrs[i] = static_cast<char*>(mmap(NULL, m_CapBuf[i].length, PROT_READ | PROT_WRITE, MAP_SHARED, m_CameraFd, m_CapBuf[i].m.offset));
        if (m_MmapedPtrs[i] == MAP_FAILED) {
            return -1;
        }
	    LOG_TRACE(("buf %d mmapped to %p\n", i, m_MmapedPtrs[i]));
    }

    return 0;
}

void CameraInputv4l::OnIdle( ) {
    if (m_State == PLAYING) {
        v4l2_buffer buf;

        Lock();

        memset(&buf, 0, sizeof(struct v4l2_buffer));
    	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	    buf.memory = V4L2_MEMORY_MMAP;
    	if (ioctl(m_CameraFd, VIDIOC_DQBUF, &buf) == -1)	{
            PostEvent(new ErrEvt("buf deq failes", CAMERA_INPUT_ERR_QUEUE, this));
            LOG_ERROR(("ERROR buf deq"));
            UnLock();
            Kick(NULL);
            return;
	    }
        
        LOG_TRACE(("got buf inx :%d %d %d %d %llu\n", buf.index, buf.sequence, buf.bytesused, buf.length, m_Clock->GetClock()));
        
        {
            static uint64_t time = m_Clock->GetClock();
            uint64_t diff = (m_Clock->GetClock() - time) / 1000000 + 1;
            (void)diff;
            LOG_TRACE(("diff %llu ms %llu fps",  diff, 1000 / diff));
            time = m_Clock->GetClock();
        }
    	
        {
            static unsigned int last_seq = buf.sequence;
            if (buf.sequence != last_seq + 1) {
                LOG_WARNING(("droppped frames: %d", buf.sequence - last_seq - 1));
            }
            last_seq = buf.sequence;
        }
        
        if (buf.bytesused != m_CurPicSize) {
            LOG_ERROR(("\ncorrupt frame frame: %d\n", buf.bytesused));
        }
        else {
            for (Pad::PadItr i=m_OutputsPads.begin(); i != m_OutputsPads.end(); ++i) {
            //TODO:add sescriptor evt
                Pad::SynchEvt tEvt(new TimeStEvt(m_Clock->GetClock()));
                while ((*i)->Write(m_MmapedPtrs[buf.index], buf.bytesused, &tEvt) == 0) {
                    pthread_yield();
                }
            }
        }
        
        if (ioctl(m_CameraFd, VIDIOC_QBUF, &buf) == -1) {
            PostEvent(new ErrEvt("buf q failes", CAMERA_INPUT_ERR_QUEUE, this));
            LOG_ERROR(("ERROR buf q"));
            UnLock();
            Kick(NULL);
            return ;
        }
    
        UnLock();
        Kick(NULL);
    }
}

};};

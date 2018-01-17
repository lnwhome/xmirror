#ifndef CAMERAINPUT_HH
#define CAMERAINPUT_HH
#include "../IZelement.hh"

#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace mfw { namespace zel {

/**
  * class CAMERAInput - defines properties of zelements which can read data from CAMERA
  *
  *
  *    generates: ErrEvt, EOSEvt, IndxEvt
  */

class CameraInput : public IZelement {
public:
    enum {
        CAMERA_INPUT_ERR_NO_DEV = 1,
        CAMERA_INPUT_ERR_NO_CAP,
        CAMERA_INPUT_ERR_NO_CAP_STREAMING,
        CAMERA_INPUT_ERR_NO_INPUT,
        CAMERA_INPUT_ERR_SIZE,
        CAMERA_INPUT_ERR_FCC,
        CAMERA_INPUT_ERR_MMAP,
        CAMERA_INPUT_ERR_QUEUE,
        CAMERA_INPUT_ERR_STREAM,
        CAMERA_INPUT_ERR_FPS
    };
    //negative means not set
    struct CameraInputDscr {
        int width, height;
        FCCType fcc;
        int fps;
        std::string busid;
    };
    
    virtual bool Querry(std::vector<CameraInput::CameraInputDscr> &v) = 0;
    CameraInput ( );

    virtual ~CameraInput ( );
    
    /*! 
    * \brief Data is read from camera at path given as argument
    *
    */
    void SetURL(const char *a_URL) {
        m_URI = std::string(a_URL);
    }

    void SetDescriptor(CameraInputDscr & a_Dscr) {
        m_Dscr = a_Dscr;
    }
protected:
    std::string m_URI;
    CameraInputDscr m_Dscr;
};

CameraInput* CreateCameraInput();

};};

#endif // CAMERAINPUT_HH

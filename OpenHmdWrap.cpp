#include "OpenHmdWrap.h"

#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include "TypesConf.h"

OpenHmdWrap::OpenHmdWrap()
    : omhdCtx{ohmd_ctx_create()}
{
    if (ohmd_ctx_probe(omhdCtx) < 0) {
        throw Error(std::string("failed to probe devices:") +
                    ohmd_ctx_get_error(omhdCtx));
    }

    ohmd_device_settings* settings = ohmd_device_settings_create(omhdCtx);
    int auto_update = 1;
    ohmd_device_settings_seti(settings, OHMD_IDS_AUTOMATIC_UPDATE,
                              &auto_update);

    hmd = ohmd_list_open_device_s(omhdCtx, 0, settings);
    if (!hmd) {
        throw Error(std::string("failed to open devices:") +
                    ohmd_ctx_get_error(omhdCtx));
    }
    ohmd_device_settings_destroy(settings);

    ohmd_device_geti(hmd, OHMD_SCREEN_HORIZONTAL_RESOLUTION, &hmd_w);
    ohmd_device_geti(hmd, OHMD_SCREEN_VERTICAL_RESOLUTION, &hmd_h);
    // viewport is half the screen
    ohmd_device_getf(hmd, OHMD_SCREEN_HORIZONTAL_SIZE, &(viewport_scale[0]));
    viewport_scale[0] /= 2.0f;
    ohmd_device_getf(hmd, OHMD_SCREEN_VERTICAL_SIZE, &(viewport_scale[1]));
    // distortion coefficients
    ohmd_device_getf(hmd, OHMD_UNIVERSAL_DISTORTION_K, &(distortion_coeffs[0]));
    ohmd_device_getf(hmd, OHMD_UNIVERSAL_ABERRATION_K, &(aberr_scale[0]));
    // calculate lens centers (assuming the eye separation is the distance
    // betweenteh lense centers)
    ohmd_device_getf(hmd, OHMD_LENS_HORIZONTAL_SEPARATION, &sep);
    ohmd_device_getf(hmd, OHMD_LENS_VERTICAL_POSITION, &(left_lens_center[1]));
    ohmd_device_getf(hmd, OHMD_LENS_VERTICAL_POSITION, &(right_lens_center[1]));
    left_lens_center[0] = viewport_scale[0] - sep / 2.0f;
    right_lens_center[0] = sep / 2.0f;
    // asume calibration was for lens view to which ever edge of screen is
    // further away from lens center
    warp_scale = (left_lens_center[0] > right_lens_center[0])
                     ? left_lens_center[0]
                     : right_lens_center[0];
    warp_adj = 1.0f;
    
    using namespace std::chrono_literals;
    for (auto i{0}; i < 150; ++i)
    {
        ohmd_ctx_update(omhdCtx);
        loge_ << "wait for 2nd screen " << i << "\n";
        auto ret = ::system("xrandr --screen 1 --output HDMI-0 --auto");
        (void)ret;
        if (::system("xrandr --screen 1 | grep \"HDMI-0\" | grep -q \"2160x1200\"") == 0)
        {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }

}

OpenHmdWrap::~OpenHmdWrap()
{
    ohmd_ctx_destroy(omhdCtx);
}


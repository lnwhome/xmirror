#pragma once

#include <openhmd.h>

class OpenHmdWrap
{
public:
    OpenHmdWrap();
    virtual ~OpenHmdWrap();

    // ohmd context + run time params
    ohmd_context* omhdCtx;
    ohmd_device* hmd;
    int hmd_w, hmd_h;
    float viewport_scale[2];
    float distortion_coeffs[4];
    float aberr_scale[3];
    float sep;
    float left_lens_center[2];
    float right_lens_center[2];
    float warp_scale;
    float warp_adj;
};

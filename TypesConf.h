#pragma once

#include <list>
#include <vector>
#include <string>
#include <math.h>
#include "Log.h"


typedef float cl_float;//WA for removed opencl
struct cl_float4//WA for removed opencl
{
    cl_float4(float x = 0, float y = 0, float z = 0, float w = 0)
        : x{x},y{y},z{z},w{w}
    {
    }

    float x,y,z,w;
};

struct Error {
    Error(std::string msg) : mMsg{msg} {}
    std::string mMsg;
};

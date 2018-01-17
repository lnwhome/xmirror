#pragma once

#include <math.h>
#include <iostream>
#include <map>
#include <memory>

#include "TypesConf.h"
#include "OpenHmdWrap.h"
#include "OpenGlWrap.h"
#include "Client.h"
#include "Xinput.h"

#define OVERSAMPLE_SCALE 2.0

GLuint compile_shader(const char* vertex, const char* fragment);

class RenderingEngine : protected OpenHmdWrap, protected OpenGlWrap
{
public:

    RenderingEngine(int argc, char** arg, std::vector<std::shared_ptr<Client> >& clients);
    virtual ~RenderingEngine();
    void run();
    void draw_text(float x, float y, float z, float scale, const char* s,
                   bool ui = false);
    cl_float4 getRotation()
    {
        return mRotation;
    }
    // basic math
    cl_float4 rotate_vertex_position(cl_float4 q_pos, cl_float4 qr);
private:
    void generateScene();
    void draw_hud(const float x, const float y);
    void draw_crosshairs(float len, float cx, float cy);
    void renderSceneToFrameBuffer(bool leftEye);
    void renderLeftRightTextures();
    bool handleEvents();
    void handleInput(SDL_Event& event);
    uint64_t calculateFps();
    std::unique_ptr<Xinput> mXinput;

private:
    // hud related
    std::map<std::string, bool> mRenderedItems;
    std::map<std::string, uint64_t> mCounters;
    // basic world coordinates
    cl_float4 lookat;
    cl_float4 whereami;
    cl_float4 mRotation;
    // helpers
    std::experimental::optional<GLuint> mList;
    uint64_t rdtsc();
    // basic math
    cl_float4 quat_conj(cl_float4 q);
    cl_float4 quat_mult(cl_float4 q1, cl_float4 q2);
    // clients
    std::vector<std::shared_ptr<Client> >& mClients;

    bool mDone; //exit main loop if true
};

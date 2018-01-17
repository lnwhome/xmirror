#include <math.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

#include <GL/glew.h>
#include <GL/glut.h>

#include "RenderingEngine.h"
#include "Client.h"

RenderingEngine::RenderingEngine(int argc, char** argv, std::vector<std::shared_ptr<Client> >& clients)
    : OpenGlWrap(OpenHmdWrap::hmd_w, OpenHmdWrap::hmd_h), 
      lookat{0, 0, 0, 0},
      whereami{0, 0, 0, 0},
      mRotation{0,0,0,1},
      mClients{clients},
      mDone{false}
{
    mRenderedItems["crosshair_overlay"] = false;

    mCounters["render_scene_time"] = 0;
    mCounters["fps"] = 0;

    glutInit(&argc, argv);

    const char* vertex;
    ohmd_gets(OHMD_GLSL_DISTORTION_VERT_SRC, &vertex);
    const char* fragment;
    ohmd_gets(OHMD_GLSL_DISTORTION_FRAG_SRC, &fragment);

    shader = compileShaders(vertex, fragment);
    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "warpTexture"), 0);
    glUniform2fv(glGetUniformLocation(shader, "ViewportScale"), 1,
                 viewport_scale);
    glUniform4fv(glGetUniformLocation(shader, "HmdWarpParam"), 1,
                 distortion_coeffs);
    glUniform3fv(glGetUniformLocation(shader, "aberr"), 1, aberr_scale);
    glUniform1f(glGetUniformLocation(shader, "WarpScale"),
                warp_scale * warp_adj);
    glUseProgram(0);

    eye_w = hmd_w / 2 * OVERSAMPLE_SCALE;
    eye_h = hmd_h * OVERSAMPLE_SCALE;
    createFbo(eye_w, eye_h, &left_fbo, &left_color_tex, &left_depth_tex);

    createFbo(eye_w, eye_h, &right_fbo, &right_color_tex, &right_depth_tex);

    generateScene();
    
    mXinput = std::make_unique<Xinput>(nullptr, 0, std::bind(&RenderingEngine::handleInput, this, std::placeholders::_1));
}

void RenderingEngine::draw_crosshairs(float len, float cx, float cy)
{
    GLint org_line_width{1};

    glClear(GL_DEPTH_BUFFER_BIT);
    glGetIntegerv(GL_LINE_WIDTH, &org_line_width);
    glLineWidth(2.0);
    glColor4f(1.0, 0.5, 0.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glBegin(GL_LINES);
    float l = len / 2.0f;
    glVertex3f(cx - l, cy, 0.0);
    glVertex3f(cx + l, cy, 0.0);
    glVertex3f(cx, cy - l, 0.0);
    glVertex3f(cx, cy + l, 0.0);
    glEnd();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glLineWidth(org_line_width);
}

void RenderingEngine::renderSceneToFrameBuffer(bool leftEye)
{
    float matrix[16];
    auto x{0.0};
    auto y{0.0};
    //calculate center for given eye
    if (leftEye)
    {
        x = 2 * left_lens_center[0] / viewport_scale[0] - 1.0f;
        y = 2 * left_lens_center[1] / viewport_scale[1] - 1.0f;
    } else
    {
        x = 2 * right_lens_center[0] / viewport_scale[0] - 1.0f;
        y = 2 * right_lens_center[1] / viewport_scale[1] - 1.0f;
    }

    // set hmd rotation, for left eye.
    glMatrixMode(GL_PROJECTION);
    ohmd_device_getf(hmd, leftEye ? OHMD_LEFT_EYE_GL_PROJECTION_MATRIX
                                  : OHMD_RIGHT_EYE_GL_PROJECTION_MATRIX,
                     matrix);
    glLoadMatrixf(matrix);

    glMatrixMode(GL_MODELVIEW);
    ohmd_device_getf(hmd, leftEye ? OHMD_LEFT_EYE_GL_MODELVIEW_MATRIX
                                  : OHMD_RIGHT_EYE_GL_MODELVIEW_MATRIX,
                     matrix);
    glLoadMatrixf(matrix);

    // Draw scene into framebuffer.
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, leftEye ? left_fbo : right_fbo);
    glViewport(0, 0, eye_w, eye_h);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glCallList(*mList);
    for (auto& client : mClients)
    {
        if (client->getList())
        {
            glCallList(*client->getList());
        }
    }
    {
        if (mRenderedItems["crosshair_overlay"]) {
            draw_crosshairs(0.1, x, y);
        }
        draw_hud(x - 0.35, y - 0.4);
    }
}

void RenderingEngine::renderLeftRightTextures()
{
    glUseProgram(shader);
    // Draw left eye
    glUniform2fv(glGetUniformLocation(shader, "LensCenter"), 1,
                 left_lens_center);
    glBindTexture(GL_TEXTURE_2D, left_color_tex);
    glBegin(GL_QUADS);
    glTexCoord2d(0, 0);
    glVertex3d(-1, -1, 0);
    glTexCoord2d(1, 0);
    glVertex3d(0, -1, 0);
    glTexCoord2d(1, 1);
    glVertex3d(0, 1, 0);
    glTexCoord2d(0, 1);
    glVertex3d(-1, 1, 0);
    glEnd();

    // Draw right eye
    glUniform2fv(glGetUniformLocation(shader, "LensCenter"), 1,
                 right_lens_center);
    glBindTexture(GL_TEXTURE_2D, right_color_tex);
    glBegin(GL_QUADS);
    glTexCoord2d(0, 0);
    glVertex3d(0, -1, 0);
    glTexCoord2d(1, 0);
    glVertex3d(1, -1, 0);
    glTexCoord2d(1, 1);
    glVertex3d(1, 1, 0);
    glTexCoord2d(0, 1);
    glVertex3d(0, 1, 0);
    glEnd();
    glUseProgram(0);
}

bool RenderingEngine::handleEvents()
{
    // events: regenerate scene
    SDL_Event event;
    auto rs0{rdtsc()};
    while (SDL_PollEvent(&event))
    {
        auto client{std::find_if(mClients.begin(), mClients.end(), [&](auto& c){return c->getEvtId() == event.type;})};
        if (client != mClients.end())
        {
            (*client)->generateScene(event, whereami, lookat, this);
        }
        if (rdtsc() - rs0 > 7'000)
        {
            logd_ << "rst " << (rdtsc() - rs0) / 1000 << "\n";
            return false;
        }
    }

    return false;
}

/// @todo multithread access
void RenderingEngine::handleInput(SDL_Event& event)
{
    if (event.type == SDL_MOUSEBUTTONDOWN ||
        event.type == SDL_MOUSEBUTTONUP)
    {
        for (const auto& cli : mClients)
        {
            cli->handleEvents(event);
        }
    }
    
    if (event.type == SDL_KEYDOWN &&
        event.key.keysym.mod != KMOD_RSHIFT)
    {
        return;
    }
    if (event.type == SDL_KEYDOWN)
    {
        switch (event.key.keysym.sym)
        {
            case SDLK_ESCAPE:
                mDone = true;
                return;
            case SDLK_UP:
                whereami.x -= lookat.x / 10;
                whereami.y -= lookat.y / 10;
                whereami.z -= lookat.z / 10;
                break;
            case SDLK_DOWN:
                whereami.x += lookat.x / 10;
                whereami.y += lookat.y / 10;
                whereami.z += lookat.z / 10;
                break;
            case SDLK_F2:
                {
                    // reset rotation and position
                    float zero[] = {0, 0, 0, 1};
                    ohmd_device_setf(hmd, OHMD_ROTATION_QUAT, zero);
                    ohmd_device_setf(hmd, OHMD_POSITION_VECTOR, zero);
                }
                break;
            case SDLK_F3:
                {
                    float ipd{0.0};
                    ohmd_device_getf(hmd, OHMD_EYE_IPD, &ipd);

                    printf("viewport_scale: [%0.4f, %0.4f]\n",
                           viewport_scale[0], viewport_scale[1]);
                    printf("lens separation: %04f\n", sep);
                    printf("IPD: %0.4f\n", ipd);
                    printf("warp_scale: %0.4f\r\n", warp_scale);
                    printf("distoriton coeffs: [%0.4f, %0.4f, %0.4f, %0.4f]\n",
                           distortion_coeffs[0], distortion_coeffs[1],
                           distortion_coeffs[2], distortion_coeffs[3]);
                    printf("aberration coeffs: [%0.4f, %0.4f, %0.4f]\n",
                           aberr_scale[0], aberr_scale[1], aberr_scale[2]);
                    printf("left_lens_center: [%0.4f, %0.4f]\n",
                           left_lens_center[0], left_lens_center[1]);
                    printf("right_lens_center: [%0.4f, %0.4f]\n",
                           right_lens_center[0], right_lens_center[1]);
                }
                break;
            case SDLK_w:  // zez
                sep += 0.001;
                left_lens_center[0] = viewport_scale[0] - sep / 2.0f;
                right_lens_center[0] = sep / 2.0f;
                break;
            case SDLK_q:
                sep -= 0.001;
                left_lens_center[0] = viewport_scale[0] - sep / 2.0f;
                right_lens_center[0] = sep / 2.0f;
                break;
            case SDLK_a:  // lense effect
                {
                    warp_adj *= 1.0 / 0.9;
                    glUseProgram(shader);
                    glUniform1f(glGetUniformLocation(shader, "WarpScale"),
                                warp_scale * warp_adj);
                    glUseProgram(0);
                }
                break;
            case SDLK_z:
                {
                    warp_adj *= 0.9;
                    glUseProgram(shader);
                    glUniform1f(glGetUniformLocation(shader, "WarpScale"),
                                warp_scale * warp_adj);
                    glUseProgram(0);
                }
                break;
            case SDLK_i:  // futher <-> near
                {
                    float ipd{0.0};
                    ohmd_device_getf(hmd, OHMD_EYE_IPD, &ipd);
                    ipd -= 0.001;
                    ohmd_device_setf(hmd, OHMD_EYE_IPD, &ipd);
                }
                break;
            case SDLK_o:
                {
                    float ipd{0.0};
                    ohmd_device_getf(hmd, OHMD_EYE_IPD, &ipd);
                    ipd += 0.001;
                    ohmd_device_setf(hmd, OHMD_EYE_IPD, &ipd);
                }
                break;
            case SDLK_d:
                {
                    static bool distorted{false};  // TODO:get it from shader
                    float undistortion_coeffs[4] = {0.0, 0.0, 0.0, 1.0};

                    glUseProgram(shader);
                    glUniform4fv(glGetUniformLocation(shader, "HmdWarpParam"),
                                 1, distorted ? distortion_coeffs : undistortion_coeffs);
                    glUseProgram(0);

                    distorted = !distorted;
                }
                break;
            case SDLK_x:
                mRenderedItems["crosshair_overlay"] =
                    !mRenderedItems["crosshair_overlay"];
                break;
            default:
                for (const auto& cli : mClients)
                {
                    cli->handleEvents(event);
                }
                break;
        }
    }
}

void RenderingEngine::run()
{
    while (!mDone)
    {
        // common scene state
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        // get new rotation/position from hmd and use it to render
        ohmd_ctx_update(omhdCtx);
        // render
        renderSceneToFrameBuffer(true);
        renderSceneToFrameBuffer(false);
        // clean up common draw state
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        // setup ortho state.
        glViewport(0, 0, hmd_w, hmd_h);
        glEnable(GL_TEXTURE_2D);
        glColor4d(1, 1, 1, 1);
        // setup simple render state
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        // textures
        renderLeftRightTextures();
        // clean up state.
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);


        auto rs0{rdtsc()};
        handleEvents();

        mCounters["render_scene_time"] = rdtsc() - rs0;
        // Da swap-dawup!
        SDL_GL_SwapWindow(GlCtx.window);

        mCounters["fps"] = calculateFps();
    }
}

RenderingEngine::~RenderingEngine()
{
    if (mList)
    {
        glDeleteLists(*mList, 1);
    }
}

uint64_t RenderingEngine::calculateFps()
{
    static auto one_sec{rdtsc()};
    static auto lfps{0};
    ++lfps;
    auto tdiff = rdtsc() - one_sec;
    if (tdiff >= 1'000'000) {
        one_sec = rdtsc();
        auto tmp{lfps};
        lfps = 0;
        return tmp;
    }
    return mCounters["fps"];
}

void RenderingEngine::draw_hud(const float x, const float y)
{
    char text[64];

    ohmd_device_getf(hmd, OHMD_ROTATION_QUAT, &mRotation.x);
    cl_float4 forw{0, 0, -1, 0};
    lookat = rotate_vertex_position(forw, mRotation);

    snprintf(text, sizeof(text),
             "Pos: %2.1f %2.1f %2.1f",
             whereami.x, whereami.y, whereami.z);
    draw_text(x, y, 0, 0.00015, text, true);

    snprintf(text, sizeof(text), "RST: %3zd ms",
             mCounters["render_scene_time"] / 1'000);
    draw_text(x, y - 0.12, 0, 0.00015, text, true);

    snprintf(text, sizeof(text), "FPS: %3zd", mCounters["fps"]);
    draw_text(x, y - 0.15, 0, 0.00015, text, true);
    for (auto& cli : mClients)
    {
        cli->generateHud(this, x,y);
    }
}

// Draws the 3D scene
void RenderingEngine::generateScene()
{
    if (mList)
    {
        glDeleteLists(*mList, 1);
    }

    mList = glGenLists(1);
    glNewList(*mList, GL_COMPILE);

    glColor4f(0, 1.0f, .25f, .25f);

    draw_text(-5, -5, +5, 0.002, "*LEFT,  DOWN, BACK");
    draw_text(-5, +5, +5, 0.002, "*LEFT,  UP,   BACK");
    draw_text(+5, +5, +5, 0.002, "*RIGHT, UP,   BACK");
    draw_text(+5, -5, +5, 0.002, "*RIGHT, DOWN, BACK");
    draw_text(-5, -5, -5, 0.002, "*LEFT,  DOWN, FRONT");
    draw_text(-5, +5, -5, 0.002, "*LEFT,  UP,   FRONT");
    draw_text(+5, +5, -5, 0.002, "*RIGHT, UP,   FRONT");
    draw_text(+5, -5, -5, 0.002, "*RIGHT, DOWN, FRONT");

    glEndList();
}

void RenderingEngine::draw_text(float x, float y, float z, float scale,
                                const char* s, bool ui)
{
    GLint org_line_width{1};
    if (ui) {
        glClear(GL_DEPTH_BUFFER_BIT);
        glGetIntegerv(GL_LINE_WIDTH, &org_line_width);
        glLineWidth(3);
        glColor4f(1.0, 0.5, 0.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
    }    
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(scale, scale, scale);
    for (auto i = 0u; i < strlen(s); ++i)
        glutStrokeCharacter(GLUT_STROKE_ROMAN, s[i]);
    glPopMatrix();
    if (ui) {
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glLineWidth(org_line_width);
    }
}

cl_float4 RenderingEngine::quat_conj(cl_float4 q)
{
    return cl_float4{-q.x, -q.y, -q.z, q.w};
}

cl_float4 RenderingEngine::quat_mult(cl_float4 q1, cl_float4 q2)
{
    cl_float4 qr;
    qr.x = (q1.w * q2.x) + (q1.x * q2.w) + (q1.y * q2.z) - (q1.z * q2.y);
    qr.y = (q1.w * q2.y) - (q1.x * q2.z) + (q1.y * q2.w) + (q1.z * q2.x);
    qr.z = (q1.w * q2.z) + (q1.x * q2.y) - (q1.y * q2.x) + (q1.z * q2.w);
    qr.w = (q1.w * q2.w) - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z);
    return qr;
}

cl_float4 RenderingEngine::rotate_vertex_position(cl_float4 q_pos, cl_float4 qr)
{
    cl_float4 qr_conj = quat_conj(qr);

    cl_float4 q_tmp = quat_mult(qr, q_pos);
    qr = quat_mult(q_tmp, qr_conj);

    return qr;
}

uint64_t RenderingEngine::rdtsc()
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
}

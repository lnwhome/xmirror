#pragma once

#include <SDL2/SDL.h>
#include <GL/gl.h>
typedef struct
{
    int w, h;
    SDL_Window* window;
    SDL_GLContext glcontext;
} gl_ctx;

class OpenGlWrap
{
public:
    OpenGlWrap(int w, int h);
    
    virtual ~OpenGlWrap();

    GLuint compileShaders(const char* vertex, const char* fragment);
    
    void createFbo(int eye_width, int eye_height, GLuint* fbo,
                   GLuint* color_tex, GLuint* depth_tex);

    void deleteFbo(GLuint* fbo, GLuint* color_tex, GLuint* depth_tex);
    
    gl_ctx GlCtx;
    GLuint left_color_tex, left_depth_tex, left_fbo;
    GLuint right_color_tex, right_depth_tex, right_fbo;
    GLuint shader;
    int eye_w;
    int eye_h;
private:
    void compileShaderSrc(GLuint shader, const char* src);
};

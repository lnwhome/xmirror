#include <iostream>
#include <GL/glew.h>

#include "OpenGlWrap.h"
#include "TypesConf.h"

OpenGlWrap::OpenGlWrap(int w, int h)
    : left_color_tex{0},
      left_depth_tex{0},
      left_fbo{0},
      right_color_tex{0},
      right_depth_tex{0},
      right_fbo{0}
{
	if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
		throw Error("SDL_Init failed");
	}

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	GlCtx.window = SDL_CreateWindow("Balls",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			w, h, SDL_WINDOW_OPENGL);
	if(GlCtx.window == NULL)
    {
		throw Error("SDL_CreateWindow failed");
	}
	GlCtx.w = w;
	GlCtx.h = h;

	GlCtx.glcontext = SDL_GL_CreateContext(GlCtx.window);
	if(GlCtx.glcontext == NULL)
    {
		throw Error("SDL_GL_CreateContext");
	}

	SDL_GL_SetSwapInterval(1);

	// Load extensions.
	glewInit();

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_ALPHA_TEST);
	glLoadIdentity();

	glShadeModel(GL_SMOOTH);
	glDisable(GL_DEPTH_TEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glEnable(GL_POLYGON_SMOOTH); 
	glLoadIdentity();

	glViewport(0, 0, GlCtx.w, GlCtx.h);

	SDL_ShowCursor(SDL_DISABLE);
	
}

OpenGlWrap::~OpenGlWrap()
{
    deleteFbo(&left_fbo, &left_color_tex, &left_depth_tex);
    deleteFbo(&right_fbo, &right_color_tex, &right_depth_tex);
    glDeleteProgram(shader);
    SDL_GL_DeleteContext(GlCtx.glcontext);  
    SDL_DestroyWindow(GlCtx.window);
    SDL_Quit();
}

void OpenGlWrap::compileShaderSrc(GLuint shader, const char* src)
{
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint status;
    GLint length;
    char log[4096] = {0};

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    glGetShaderInfoLog(shader, 4096, &length, log);
    if (status == GL_FALSE)
    {
        throw Error(std::string("compile failed") + log);
    }
}


GLuint OpenGlWrap::compileShaders(const char* vertex, const char* fragment)
{
    // Create the handels
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    GLuint programShader = glCreateProgram();

    // Attach the shaders to a program handel.
    glAttachShader(programShader, vertexShader);
    glAttachShader(programShader, fragmentShader);

    // Load and compile the Vertex Shader
    compileShaderSrc(vertexShader, vertex);

    // Load and compile the Fragment Shader
    compileShaderSrc(fragmentShader, fragment);

    // The shader objects are not needed any more,
    // the programShader is the complete shader to be used.
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glLinkProgram(programShader);

    GLint status;
    GLint length;
    char log[4096] = {0};

    glGetProgramiv(programShader, GL_LINK_STATUS, &status);
    glGetProgramInfoLog(programShader, 4096, &length, log);
    if (status == GL_FALSE)
    {
        throw Error(std::string("link failed") + log);
    }

    return programShader;
}

void OpenGlWrap::createFbo(int eye_width, int eye_height, GLuint* fbo,
                                GLuint* color_tex, GLuint* depth_tex)
{
    glGenTextures(1, color_tex);
    glGenTextures(1, depth_tex);
    glGenFramebuffers(1, fbo);

    glBindTexture(GL_TEXTURE_2D, *color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, eye_width, eye_height, 0, GL_RGBA,
                 GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, *depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, eye_width, eye_height,
                 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           *color_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           *depth_tex, 0);

    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) !=
        GL_FRAMEBUFFER_COMPLETE_EXT) {
        throw Error("failed to create fbo");
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void OpenGlWrap::deleteFbo(GLuint* fbo, GLuint* color_tex, GLuint* depth_tex)
{
    glDeleteFramebuffers(1, fbo);
    glDeleteTextures(1, color_tex);
    glDeleteTextures(1, depth_tex);
}

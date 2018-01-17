#pragma once

#include <experimental/optional>
#include <thread>
#include <string>
#include <map>

#include <SDL2/SDL.h>
#include <GL/gl.h>

#include "TypesConf.h"

class RenderingEngine;
class Client
{
public:
    Client()
        : mGenerateSceneEventId{SDL_RegisterEvents(1)}
    {
    
    }
    
    virtual ~Client()
    {
    
    }
    virtual bool run(RenderingEngine* renderingEngine, bool* exit) = 0;
    virtual bool join() = 0;
    //tell renderer to call us(generateScene(...)) from opengl thread
    virtual void requestSceneGeneration(int code, void* data)
    {
        if (mGenerateSceneEventId == ((Uint32)-1))
        {
            throw Error("Unable to send event");
        }
        SDL_Event event;
        SDL_memset(&event, 0, sizeof(event)); /* or SDL_zero(event) */
        event.type = mGenerateSceneEventId;
        event.user.code = code;
        event.user.data1 = this;
        event.user.data2 = data;
        SDL_PushEvent(&event);
    }
    
    virtual void generateHud(RenderingEngine* renderingEngine, cl_float x, cl_float y)
    {
        (void)renderingEngine;
        (void)x;
        (void)y;
    }
    
    virtual void handleEvents(SDL_Event& event)
    {
        (void)event;
        //overload
    }
    
    virtual void generateScene(const SDL_Event& event, cl_float4& whereami, cl_float4& lookat, RenderingEngine* renderingEngine) = 0;
    
    Uint32 getEvtId() const
    {
        return mGenerateSceneEventId;
    }

    virtual std::experimental::optional<GLuint> getList() const
    {
        return mList;
    }
    
protected:    
    std::unique_ptr<std::thread> mThread;
    
    std::experimental::optional<GLuint> mList;
    Uint32 mGenerateSceneEventId;

    std::map<std::string, bool> mRenderedItems;
    std::map<std::string, uint64_t> mCounters;
};

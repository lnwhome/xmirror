#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <future>
#include <iostream>
#include <regex>
#include <thread>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/thread/thread_time.hpp>

#include "Client.h"
#include "TypesConf.h"
#include "geometry.h"
#include "RenderingEngine.h"

#include <GL/glu.h>
#include <GL/glext.h>

class Mirror {
public:
    typedef std::experimental::optional<GLuint> OptionalTexture;
    Mirror();
    ~Mirror();
    std::string name;
    Display* display;
    Window window;
    cl_float4 pos;
    cl_float4 rot;
    size_t width, height;
    cl_float scale;
    uint8_t transparency;
    bool haveFocus;
    std::chrono::milliseconds updateInterval;
    std::chrono::system_clock::time_point nextUpdate;
    boost::interprocess::interprocess_semaphore
        requests;
    boost::interprocess::interprocess_semaphore responses;
    bool toBeDeleted;
    std::thread worker;
    uint64_t era;
    std::vector<uint8_t> mImage;
    OptionalTexture mTexture;
    GLuint mPbo;
    //size of texture
    size_t mTextWidth;
    size_t mTextHeight;
    std::shared_ptr<XFixesCursorImage> mCursor;
    enum
    {
        ld = 0,
        lu = 1,
        ru = 2,
        rd = 3
    } Corners;
    Vec3f mConer[4];
protected:
    void* thrFnc(Mirror* me);
    void burnMousePointer(Display* display, Window window, XWindowAttributes gwa);
};

class XServerMirror : public Client {
   public:
    XServerMirror(const std::string& masterListName,
                  const std::string& blackListName);

    virtual ~XServerMirror()
    {
        std::cout << "XServerMirror going down\n";
        
        mThread->join(); //external must set exit otherwise we hang here
        
        try
        {
            write_json(mMasterListName, serializeList(mMasterList));
            write_json(mBlackListName, serializeList(mBlackList));
        } catch (...)
        {
            logw_ << "Master/Balck list not created\n";
        }        
        
        //no workers running
        // destroy all workers do not risk accessing invalid mDisplay
        mMasterList.clear();
        mBlackList.clear();
       
        XCloseDisplay(mDisplay);
    }

    bool run(RenderingEngine* renderingEngine, bool* exit) {
        mThread = std::make_unique<std::thread>(&XServerMirror::thrFnc, this,
                                                renderingEngine, exit);

        return true;
    }

    bool join()
    {
        return true;
    }

    std::chrono::system_clock::time_point findSleepTime();

    void UpdateMasterList(Display* display, Window win);

    void* thrFnc(RenderingEngine* renderingEngine, bool* exit) {
        (void)renderingEngine;

        for (; !*exit;)
        {
            logd_ << "------------------------------------------------\n";

            std::this_thread::sleep_until(findSleepTime());

            UpdateMasterList(mDisplay, mRootWindow);
            
            std::list<std::shared_ptr<Mirror>> waitList;
            for (auto& mirror : mMasterList) {
                if (mirror->nextUpdate < std::chrono::system_clock::now()) {
                    mirror->requests.post();
                    waitList.push_back(mirror);
                    mirror->nextUpdate = std::chrono::system_clock::now() +
                                         (mirror->haveFocus ? mirror->updateInterval / 4 : mirror->updateInterval);
                }
            }
            
            if (waitList.empty())
            {
                continue;
            }

            for(auto& mirror : waitList)
            {
                mirror->responses.wait();
                requestSceneGeneration(true, mirror.get());
            }
            requestSceneGeneration(false, nullptr);
            if (!mRequestSceneGeneration.timed_wait(boost::get_system_time() + boost::posix_time::milliseconds(500)))
            {
                loge_ << "Server not responding\n";
            }
        }
        logi_ << "Master thread exited\n";

        return nullptr;
    }
    
    virtual void generateHud(RenderingEngine* renderingEngine, cl_float x, cl_float y);
    
    bool rayTriangleIntersect(
        const Vec3f &orig, const Vec3f &dir,
        const Vec3f &v0, const Vec3f &v1, const Vec3f &v2,
        float &t, float &u, float &v)
    {
        Vec3f v0v1 = v1 - v0;
        Vec3f v0v2 = v2 - v0;
        Vec3f pvec = dir.crossProduct(v0v2);
        float det = v0v1.dotProduct(pvec);
        // if the determinant is negative the triangle is backfacing
        // if the determinant is close to 0, the ray misses the triangle
        if (det < kEpsilon) return false;
        
        float invDet = 1 / det;

        Vec3f tvec = orig - v0;
        u = tvec.dotProduct(pvec) * invDet;
        if (u < 0 || u > 1) return false;

        Vec3f qvec = tvec.crossProduct(v0v1);
        v = dir.dotProduct(qvec) * invDet;
        if (v < 0 || u + v > 1) return false;
        
        t = v0v2.dotProduct(qvec) * invDet;
        
        return t > kEpsilon;
    }
    
    void Upload(Mirror* mirror)
    {
        uint32_t* img = (uint32_t*)(mirror->mImage.data());
        if (mirror->mTexture)
        {
            glBindTexture(GL_TEXTURE_2D, *mirror->mTexture);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mirror->mPbo);
            if (mirror->width != mirror->mTextWidth || mirror->height != mirror->mTextHeight)
            {
                glBindTexture(GL_TEXTURE_2D, 0);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); //TODO: needed ? ok ?
                glDeleteTextures(1, &*mirror->mTexture);
                glDeleteBuffers(1, &mirror->mPbo);
                mirror->mTexture = {};
            }
        }

        if (!mirror->mTexture)
        {
            GLuint texture;
            glGenTextures(1, &texture);
            mirror->mTexture = texture;
            glBindTexture(GL_TEXTURE_2D, *mirror->mTexture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mirror->width, mirror->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, img);
            glBindTexture(GL_TEXTURE_2D, 0);
            mirror->mTextWidth = mirror->width;
            mirror->mTextHeight = mirror->height;

            glGenBuffers(1, &mirror->mPbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mirror->mPbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, mirror->width * mirror->height * 4, 0, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }else
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mirror->width, mirror->height, GL_BGRA, GL_UNSIGNED_BYTE, 0);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, mirror->width * mirror->height * 4, 0, GL_DYNAMIC_DRAW);
            GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr)
            {
                
                ::memcpy(ptr, img, mirror->width * mirror->height * 4);
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release pointer to mapping buffer
            }else
            {
                loge_ << "failed to map PBO\n";
            }
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
    }
    
    virtual void generateScene(const SDL_Event& event, cl_float4& whereami, cl_float4& lookat, RenderingEngine* renderingEngine)
    {
        Mirror* mirror = static_cast<Mirror*>(event.user.data2);
        bool upload = static_cast<bool>(event.user.code);
        
        if (upload && mirror != nullptr && mirror->mImage.size() > 0)
        {
            Upload(mirror);
            return;
        }else
        {
            logw_ << "Update Failed \n";
        }
        
        if (!mList)
        {
            mList = glGenLists(1);
            // TODO: where to delete it within gl
            // ceontextglDeleteLists(*mList, 1);
        }

        glNewList(*mList, GL_COMPILE);
        glEnable(GL_TEXTURE_2D);
        for(auto& mirror : mMasterList)
        {
            if (mirror->pos.w == 0)
            {
                mirror->pos = (Vec3f(lookat) * 1 - Vec3f(whereami)).get();
                mirror->pos.w = 1.0;
                mirror->rot = renderingEngine->getRotation();
                logi_ << "Auto position " << mirror->name << " " << mirror->window << "\n";
            }
            logd_ << "Rendering " << mirror->name << " " << mirror->window << " " << mirror->mConer[Mirror::lu].x << " " << mirror->mConer[Mirror::lu].y << " " << mirror->mConer[Mirror::lu].z << "\n";
            logd_ << "Rendering " << mirror->name << " " << mirror->window << " " << mirror->width << " " << mirror->height << "\n";
        
            glBindTexture(GL_TEXTURE_2D, *mirror->mTexture);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            //glPushMatrix();
            glBegin(GL_QUADS);
            glTexCoord2f(0.0, 0.0);
            auto scaledHalfWidth = mirror->width / 2 * mirror->scale;
            auto scaledHalfHeight = mirror->height / 2 * mirror->scale;
            mirror->mConer[Mirror::lu] = renderingEngine->rotate_vertex_position({-scaledHalfWidth, +scaledHalfHeight, -5.0f}, mirror->rot);
            mirror->mConer[Mirror::lu] += Vec3f(whereami);
            mirror->mConer[Mirror::lu] += Vec3f(mirror->pos);
            glVertex3f(mirror->mConer[Mirror::lu].x, mirror->mConer[Mirror::lu].y, mirror->mConer[Mirror::lu].z);
            glTexCoord2f(0.0, 1.0);
            mirror->mConer[Mirror::ld] = renderingEngine->rotate_vertex_position({-scaledHalfWidth, -scaledHalfHeight, -5.0f}, mirror->rot);
            mirror->mConer[Mirror::ld] += Vec3f(whereami);
            mirror->mConer[Mirror::ld] += Vec3f(mirror->pos);
            glVertex3f(mirror->mConer[Mirror::ld].x, mirror->mConer[Mirror::ld].y, mirror->mConer[Mirror::ld].z);
            glTexCoord2f(1.0, 1.0);
            mirror->mConer[Mirror::rd] = renderingEngine->rotate_vertex_position({+scaledHalfWidth, -scaledHalfHeight, -5.0f}, mirror->rot);
            mirror->mConer[Mirror::rd] += Vec3f(whereami);
            mirror->mConer[Mirror::rd] += Vec3f(mirror->pos);
            glVertex3f(mirror->mConer[Mirror::rd].x, mirror->mConer[Mirror::rd].y, mirror->mConer[Mirror::rd].z);
            glTexCoord2f(1.0, 0.0);
            mirror->mConer[Mirror::ru] = renderingEngine->rotate_vertex_position({+scaledHalfWidth, +scaledHalfHeight, -5.0f}, mirror->rot);
            mirror->mConer[Mirror::ru] += Vec3f(whereami);
            mirror->mConer[Mirror::ru] += Vec3f(mirror->pos);
            glVertex3f(mirror->mConer[Mirror::ru].x, mirror->mConer[Mirror::ru].y, mirror->mConer[Mirror::ru].z);
            glEnd();
            //glPopMatrix();
        }
        glDisable(GL_TEXTURE_2D);
        glEndList();
        
        mRequestSceneGeneration.post();
        static auto lastWhereAmI{whereami};
        if (!mRenderedItems["dragmode"])
        {
            Vec3f orig(0);//world moves around us
            Vec3f dir(lookat);
            std::map<float, std::shared_ptr<Mirror>> zorder;
            for (auto& mirror : mMasterList)
            {
                Vec3f ld(mirror->mConer[Mirror::ld]);
                Vec3f rd(mirror->mConer[Mirror::rd]);
                Vec3f lu(mirror->mConer[Mirror::lu]);
                Vec3f ru(mirror->mConer[Mirror::ru]);
                if (rayTriangleIntersect(orig, dir, ld, rd, lu, t, u, v) ||
                    rayTriangleIntersect(orig, dir, rd, ru, lu, t, u, v))
                {
                    zorder[t] = mirror;
                }
            }
            mMirrorWithFocus = zorder.size() ? (*zorder.begin()).second : mMirrorWithFocus;
            
            Window windowWithFocus;
            int rev;
            XGetInputFocus(mDisplay, &windowWithFocus, &rev);
            if (mDisplay != nullptr &&
                mMirrorWithFocus &&
                windowWithFocus != mMirrorWithFocus->window)
            {
                logi_ << windowWithFocus << "\n";
                XSetInputFocus(mDisplay,
                               mMirrorWithFocus->window,
                               rev,
                               CurrentTime);
                XMapRaised(mDisplay,  mMirrorWithFocus->window);
                
                for(auto& mirror : mMasterList)
                {
                    mirror->haveFocus = (mirror->window == mMirrorWithFocus->window); 
                }
            }
        }
        else
        {
            if (mRenderedItems["dragmode"] && mMirrorWithFocus)
            {
                auto length = (Vec3f(lastWhereAmI) + Vec3f(mMirrorWithFocus->pos)).length();

                mMirrorWithFocus->pos = (Vec3f(lookat) * length - Vec3f(whereami)).get();
                mMirrorWithFocus->pos.w = 1.0;
                mMirrorWithFocus->rot = renderingEngine->getRotation();
            }
        }
        lastWhereAmI = whereami;
    }

    void handleEvents(SDL_Event& event);
    std::experimental::optional<Window> getWindowIdFromName(const std::string& name);
private:
    boost::property_tree::ptree serializeList(
        const std::list<std::shared_ptr<Mirror>>& list);
    std::list<std::shared_ptr<Mirror>> deSerializeList(
        const boost::property_tree::ptree& tree);
    std::string mMasterListName;
    std::string mBlackListName;
    std::list<std::shared_ptr<Mirror>> mMasterList;
    std::list<std::shared_ptr<Mirror>> mBlackList;
    uint64_t mEra;
    Display* mDisplay;
    Window mRootWindow;

    int mWidth;
    int mHeight;
    boost::interprocess::interprocess_semaphore mRequestSceneGeneration;
    
    float t, u, v;
    std::shared_ptr<Mirror> mMirrorWithFocus;
};


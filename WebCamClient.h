#pragma once

#include <cstdio>
#include <fstream>
#include <iostream>

#include "TypesConf.h"
#include "Client.h"

#include <IZelement.hh>
#include "CameraInput/CameraInput.hh"
#include "GenericWrapper/GenericWrapper.hh"

extern uint64_t rdtsc();

using namespace mfw::zel;

class WebCamera : public Client
{
public:
    typedef std::experimental::optional<GLuint> OptionalTexture;
    WebCamera(const std::string& cameraLocation, bool hudMode = false)
        : mCameraInput{CreateCameraInput()},
          mWrapper{dynamic_cast<GenericWrapper*>(CreateGenericWrapper())},
          mCameraLocation{cameraLocation},
          mHudMode{hudMode}
    {
        mWrapper->SetFunctorIN(boost::bind(&WebCamera::DataEntryPoit, this, _1, _2));
        mCameraInput->SetClock(&m_Clock);
    }
    
   virtual ~WebCamera()
    {
        delete mWrapper;
        delete mCameraInput;
    }

    uint8_t clamp(float x)
    {
        if (x > 255)
        {
            return 255;
        }
        if (x < 0)
        {
            return 0;
        }

        return (uint8_t)x;
    }
    
    void DataEntryPoit(uint8_t* a_Buf, uint32_t& a_Len)
    {
        uint32_t* img = new uint32_t[mDscr.width * mDscr.height];
        for (auto i{0u}; i < a_Len; i+=4)
        {
            float y1 = a_Buf[i + 0];
            float u = a_Buf[i + 1];
            float y2 = a_Buf[i + 2];
            float v = a_Buf[i + 3];
            auto b1 = clamp(1.164 * (y1 - 16) + 2.018 * (u - 128));
            auto g1 = clamp(1.164 * (y1 - 16) - 0.813 * (v - 128) - 0.391 * (u - 128));
            auto r1 = clamp(1.164 * (y1 - 16) + 1.596 * (v - 128));
            auto b2 = clamp(1.164 * (y2 - 16) + 2.018 * (u - 128));
            auto g2 = clamp(1.164 * (y2 - 16) - 0.813 * (v - 128) - 0.391 * (u - 128));
            auto r2 = clamp(1.164 * (y2 - 16) + 1.596 * (v - 128));

            auto x = (i/2) % mDscr.width;
            auto y = mDscr.height - (i/2) / mDscr.width - 1;
            img[y * mDscr.width + x + 0] = 0x80000000 + (b1 << 16) + (g1 << 8) + (r1);
            img[y * mDscr.width + x + 1] = 0x80000000 + (b2 << 16) + (g2 << 8) + (r2);
        }
        requestSceneGeneration(0, img);
    }
    
    bool run(RenderingEngine* renderingEngine, bool* exit)
    {
        mThread = std::make_unique<std::thread>(&WebCamera::thrFnc, this, renderingEngine, exit);

        return true;
    }

    bool join()
    {
        mThread->join();

        return true;
    }
    
    void* thrFnc(RenderingEngine* renderingEngine, bool* exit)
    {
        (void)renderingEngine;

        ssize_t videoSourceNo{-1};
        for (;!*exit;)
        {
            if (videoSourceNo == -1)
            {
                for (ssize_t j=0; j<10;++j)
                {
                    mCameraInput->SetURL(std::string("/dev/video" + std::to_string(j)).c_str());
                    std::vector<CameraInput::CameraInputDscr> v;
                    mCameraInput->Querry(v);
                    if (0 == v.size())
                    {
                        continue;
                    } 

                    for (size_t i = 0;i < v.size();++i)
                    {
                        std::cout << i << " "
                                  << v[i].width << " "
                                  << v[i].height << " "
                                  << (v[i].fcc >> 0) << " "
                                  << (v[i].fcc >> 8) << " "
                                  << (v[i].fcc >> 16) << " "
                                  << (v[i].fcc >> 24) << " "
                                  << v[i].fps << " "
                                  << "[" << v[i].busid << "]"
                                  << "\n";
                    }

                    if (v[0].busid == mCameraLocation ||
                        mCameraLocation == "ANY")
                    {
                        mDscr = v[0];
                        videoSourceNo = j;
                        break;
                    }
                }

                if (videoSourceNo == -1)
                {
                    std::cout << "No video source";
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                mCameraInput->SetURL(std::string("/dev/video" + std::to_string(videoSourceNo)).c_str());
                mCameraInput->SetDescriptor(mDscr);
                Pad* cameraOutPad = mCameraInput->GetPad(OUT);
                Pad* wrapperInput = mWrapper->GetPad(IN);
                cameraOutPad->ConnectInput(wrapperInput);

                mWrapper->SetState(READY);
                while (mWrapper->GetState() != READY) { std::this_thread::sleep_for(std::chrono::milliseconds(50));}
                mCameraInput->SetState(READY);
                while (mCameraInput->GetState() != READY) { std::this_thread::sleep_for(std::chrono::milliseconds(50));}
                mWrapper->SetState(PLAYING);
                while (mWrapper->GetState() != PLAYING) { std::this_thread::sleep_for(std::chrono::milliseconds(50));}
                mCameraInput->SetState(PLAYING);
                while (mCameraInput->GetState() != PLAYING) { std::this_thread::sleep_for(std::chrono::milliseconds(50));}
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            //TODO:: handle camera reconnect
        }
        std::cout << "set camera to zero\n";
        mCameraInput->SetState(ZERO);
        std::cout << "wait camera to zero\n";
        while (mCameraInput->GetState() != ZERO) { std::this_thread::sleep_for(std::chrono::milliseconds(50));}
        std::cout << "set wrapper to zero\n";
        mWrapper->SetState(ZERO);
        std::cout << "wait wrapper to zero\n";
        while (mWrapper->GetState() != ZERO) { std::this_thread::sleep_for(std::chrono::milliseconds(50));}
        
        std::cout << "client exited\n";

        return nullptr;
    }
    
    virtual void generateScene(const SDL_Event& event, cl_float4& whereami, cl_float4& lookat, RenderingEngine* renderingEngine)
    {
        (void)lookat;
        (void)whereami;
        (void)renderingEngine;
        if (event.user.data2 != nullptr)
        {
            uint32_t* img = (uint32_t*)event.user.data2;
            bool alpha = true;
            
            if (mTexture)
            {
                int w,h;
                glBindTexture(GL_TEXTURE_2D, *mTexture);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
                glBindTexture(GL_TEXTURE_2D, 0);
                if (mDscr.width != w || mDscr.height != h)
                {
                    glDeleteTextures(1, &*mTexture);
                    mTexture = {};

                }
            }

            if (!mTexture)
            {
                GLuint texture;
                glGenTextures(1, &texture);
                mTexture = texture;
                glBindTexture(GL_TEXTURE_2D, *mTexture);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }
            glBindTexture(GL_TEXTURE_2D, *mTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, alpha ? GL_RGBA8 : GL_RGB8, mDscr.width,
                         mDscr.height, 0, alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
                         img);

            if (!mList)
            {
                mList = glGenLists(1);
                //TODO: where to delete it within gl ceontextglDeleteLists(*mList, 1);
            }
            glNewList(*mList, GL_COMPILE);
            
            if (!mHudMode)
            {
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                glBindTexture(GL_TEXTURE_2D, *mTexture);// this is needed
                glEnable(GL_TEXTURE_2D);
                glPushMatrix();
                glTranslatef(whereami.x, whereami.y, whereami.z);
                glBegin(GL_QUADS);
                glTexCoord2f(0.0, 0.0);
                glVertex3f(-5.0, -5.0, +5.0);
                glTexCoord2f(0.0, 1.0);
                glVertex3f(-5.0, -5.0, -5.0);
                glTexCoord2f(1.0, 1.0);
                glVertex3f(+5.0, -5.0, -5.0);
                glTexCoord2f(1.0, 0.0);
                glVertex3f(+5.0, -5.0, +5.0);
                glEnd();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
            }
            glEndList();

            delete[] img;
        }
    }
    
    void generateHud(RenderingEngine* renderingEngine, cl_float x, cl_float y)
    {
        (void)renderingEngine;

        if (!(mTexture && mHudMode))
        {
            return;
        }
        glClear(GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, *mTexture);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        
        auto shiftx = +0.3;
        auto shifty = -0.3;
        auto scale = 0.5;
        auto w = 1 * scale;
        auto h = 0.75 * scale;
        glTexCoord2f(0.0, 0.0);
        glVertex3f(x - 0 + shiftx, y - 0 + shifty, 0.0);
        glTexCoord2f(0.0, 1.0);
        glVertex3f(x - 0 + shiftx, y + h + shifty, 0.0);
        glTexCoord2f(1.0, 1.0);
        glVertex3f(x + w + shiftx, y + h + shifty, 0.0);
        glTexCoord2f(1.0, 0.0);
        glVertex3f(x + w + shiftx, y - 0 + shifty, 0.0);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

private:
    ZClock m_Clock;
    CameraInput* mCameraInput;
    GenericWrapper* mWrapper;

    std::string mCameraLocation;
    OptionalTexture mTexture;
    CameraInput::CameraInputDscr mDscr;
    bool mHudMode;
};

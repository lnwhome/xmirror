
/*
 * Copyright © 2009 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xutil.h>
#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Log.h"


static void print_rawevent(XIRawEvent *event)
{
    int i;
    double *val, *raw_val;

    logd_ << "    device: " << event->deviceid << " (" << event->sourceid << ")\n";
    logd_ << "    detail: " << event->detail << "\n";

    logd_ << "    valuators:\n";
    val = event->valuators.values;
    raw_val = event->raw_values;
    for (i = 0; i < event->valuators.mask_len * 8; i++)
        if (XIMaskIsSet(event->valuators.mask, i))
            logd_ << "         " << i << ": " << *val++ << " (" << *raw_val++ << ")\n";
    logd_ << "\n";
}

static const char* type_to_name(int evtype)
{
    const char *name;

    switch(evtype) {
        case XI_DeviceChanged:    name = "DeviceChanged";       break;
        case XI_KeyPress:         name = "KeyPress";            break;
        case XI_KeyRelease:       name = "KeyRelease";          break;
        case XI_ButtonPress:      name = "ButtonPress";         break;
        case XI_ButtonRelease:    name = "ButtonRelease";       break;
        case XI_Motion:           name = "Motion";              break;
        case XI_Enter:            name = "Enter";               break;
        case XI_Leave:            name = "Leave";               break;
        case XI_FocusIn:          name = "FocusIn";             break;
        case XI_FocusOut:         name = "FocusOut";            break;
        case XI_HierarchyChanged: name = "HierarchyChanged";    break;
        case XI_PropertyEvent:    name = "PropertyEvent";       break;
        case XI_RawKeyPress:      name = "RawKeyPress";         break;
        case XI_RawKeyRelease:    name = "RawKeyRelease";       break;
        case XI_RawButtonPress:   name = "RawButtonPress";      break;
        case XI_RawButtonRelease: name = "RawButtonRelease";    break;
        case XI_RawMotion:        name = "RawMotion";           break;
        case XI_TouchBegin:       name = "TouchBegin";          break;
        case XI_TouchUpdate:      name = "TouchUpdate";         break;
        case XI_TouchEnd:         name = "TouchEnd";            break;
        case XI_RawTouchBegin:    name = "RawTouchBegin";       break;
        case XI_RawTouchUpdate:   name = "RawTouchUpdate";      break;
        case XI_RawTouchEnd:      name = "RawTouchEnd";         break;
        default:
                                  name = "unknown event type"; break;
    }
    return name;
}

class Xinput
{
public:
    Xinput(Display* display, Window window, std::function<void(SDL_Event&)> callback)
        : mDisplay{display},
          mWindow{window},
          mCb{callback},
          mXiOpcode{-1},
          mExit{false}
    {
        mDisplay = XOpenDisplay(":0.0");// input from scrren zero
        //XSetErrorHandler(handlerX11);
        mWindow = DefaultRootWindow(mDisplay);

        mThread = std::make_unique<std::thread>(&Xinput::main, this);
    }

    ~Xinput()
    {
        mExit = true;
        mThread->join();

        XCloseDisplay(mDisplay);
    }
    
    void main()
    {
        XIEventMask mask[2];
        XIEventMask *m;
        int deviceid = -1;

        int event, error;
        if (!XQueryExtension(mDisplay, "XInputExtension", &mXiOpcode, &event, &error)) {
            printf("X Input extension not available.\n");
            exit(-1);
        }

        setvbuf(stdout, NULL, _IOLBF, 0);

        /* Select for motion events */
        m = &mask[0];
        m->deviceid = (deviceid == -1) ? XIAllDevices : deviceid;
        m->mask_len = XIMaskLen(XI_LASTEVENT);
        m->mask = (unsigned char*)calloc(m->mask_len, sizeof(char));
        XISetMask(m->mask, XI_ButtonPress);
        XISetMask(m->mask, XI_ButtonRelease);
        XISetMask(m->mask, XI_KeyPress);
        XISetMask(m->mask, XI_KeyRelease);
        XISetMask(m->mask, XI_Motion);
        XISetMask(m->mask, XI_DeviceChanged);
        XISetMask(m->mask, XI_Enter);
        XISetMask(m->mask, XI_Leave);
        XISetMask(m->mask, XI_FocusIn);
        XISetMask(m->mask, XI_FocusOut);
        if (m->deviceid == XIAllDevices)
            XISetMask(m->mask, XI_HierarchyChanged);
        XISetMask(m->mask, XI_PropertyEvent);

        m = &mask[1];
        m->deviceid = (deviceid == -1) ? XIAllMasterDevices : deviceid;
        m->mask_len = XIMaskLen(XI_LASTEVENT);
        m->mask = (unsigned char*)calloc(m->mask_len, sizeof(char));
        XISetMask(m->mask, XI_RawKeyPress);
        XISetMask(m->mask, XI_RawKeyRelease);
        XISetMask(m->mask, XI_RawButtonPress);
        XISetMask(m->mask, XI_RawButtonRelease);
        XISetMask(m->mask, XI_RawMotion);

        XISelectEvents(mDisplay, mWindow, &mask[0], 2);
        XSync(mDisplay, False);

        free(mask[0].mask);
        free(mask[1].mask);

        while(mExit == false)
        {
            XEvent ev;
            XGenericEventCookie *cookie = (XGenericEventCookie*)&ev.xcookie;
            XNextEvent(mDisplay, (XEvent*)&ev);

            if (XGetEventData(mDisplay, cookie) &&
                cookie->type == GenericEvent &&
                cookie->extension == mXiOpcode)
            {
                logd_ << "EVENT type " << cookie->evtype << " (" << type_to_name(cookie->evtype) << ")\n";
                switch (cookie->evtype)
                {
                    case XI_DeviceChanged:
                        break;
                    case XI_HierarchyChanged:
                        break;
                    case XI_ButtonPress:
                        {
                            XIDeviceEvent* revent = (XIDeviceEvent*)cookie->data;
                            SDL_Event event;
                            event.type = SDL_MOUSEBUTTONDOWN;
                            event.button.timestamp = 0;
                            event.button.windowID = 0;
                            event.button.button = (revent->detail == 1) ? SDL_BUTTON_LEFT : ((revent->detail == 2) ? SDL_BUTTON_MIDDLE : SDL_BUTTON_RIGHT);
                            event.button.state = SDL_PRESSED;
                            event.button.clicks = 1;
                            event.button.x = 0;
                            event.button.y = 0;
                            mCb(event);
                        }
                        break;
                    case XI_ButtonRelease:
                        {
                            XIDeviceEvent* revent = (XIDeviceEvent*)cookie->data;
                            SDL_Event event;
                            event.type = SDL_MOUSEBUTTONUP;
                            event.button.timestamp = 0;
                            event.button.windowID = 0;
                            event.button.button = (revent->detail == 1) ? SDL_BUTTON_LEFT : ((revent->detail == 2) ? SDL_BUTTON_MIDDLE : SDL_BUTTON_RIGHT);
                            event.button.state = SDL_RELEASED;
                            event.button.clicks = 1;
                            event.button.x = 0;
                            event.button.y = 0;
                            mCb(event);
                        }
                        break;
                    case XI_KeyPress:
                        {
                            XIDeviceEvent* revent = (XIDeviceEvent*)cookie->data;
                            SDL_Event event;
                            event.type = SDL_KEYDOWN;
                            event.key.keysym.mod = (revent->mods.effective & 0x01) ? KMOD_RSHIFT : 0;
                            switch(revent->detail)
                            {
                                case 9://esc
                                    event.key.keysym.sym = SDLK_ESCAPE;
                                    mCb(event);
                                    break;
                                case 111:
                                    event.key.keysym.sym = SDLK_UP;
                                    mCb(event);
                                    break;
                                case 116:
                                    event.key.keysym.sym = SDLK_DOWN;
                                    mCb(event);
                                    break;
                                case 112:
                                    event.key.keysym.sym = SDLK_PAGEUP;
                                    mCb(event);
                                    break;
                                case 117:
                                    event.key.keysym.sym = SDLK_PAGEDOWN;
                                    mCb(event);
                                    break;
                                case 69:
                                    event.key.keysym.sym = SDLK_F3;
                                    mCb(event);
                                    break;
                                case 25:
                                    event.key.keysym.sym = SDLK_w;
                                    mCb(event);
                                    break;
                                case 24:
                                    event.key.keysym.sym = SDLK_q;
                                    mCb(event);
                                    break;
                                case 38:
                                    event.key.keysym.sym = SDLK_a;
                                    mCb(event);
                                    break;
                                case 52:
                                    event.key.keysym.sym = SDLK_z;
                                    mCb(event);
                                    break;
                                case 31:
                                    event.key.keysym.sym = SDLK_i;
                                    mCb(event);
                                    break;
                                case 32:
                                    event.key.keysym.sym = SDLK_o;
                                    mCb(event);
                                    break;
                                case 40:
                                    event.key.keysym.sym = SDLK_d;
                                    mCb(event);
                                    break;
                                case 41:
                                    event.key.keysym.sym = SDLK_f;
                                    mCb(event);
                                    break;
                                case 53:
                                    event.key.keysym.sym = SDLK_x;
                                    mCb(event);
                                    break;
                                case 54:
                                    event.key.keysym.sym = SDLK_c;
                                    mCb(event);
                                    break;
                                case 56:
                                    event.key.keysym.sym = SDLK_b;
                                    mCb(event);
                                    break;
                            }
                        }
                        break;
                    case XI_RawKeyPress:
                    case XI_RawKeyRelease:
                    case XI_RawButtonPress:
                    case XI_RawButtonRelease:
                    case XI_RawMotion:
                    case XI_RawTouchBegin:
                    case XI_RawTouchUpdate:
                    case XI_RawTouchEnd:
                        print_rawevent((XIRawEvent*)cookie->data);
                        break;
                    case XI_Enter:
                    case XI_Leave:
                    case XI_FocusIn:
                    case XI_FocusOut:
                        break;
                    case XI_PropertyEvent:
                        break;
                    default:
                        break;
                }
            }

            XFreeEventData(mDisplay, cookie);
        }
    }
protected:
    Display* mDisplay;
    Window mWindow;
    std::function<void(SDL_Event&)> mCb;
    int mXiOpcode;
    std::unique_ptr<std::thread> mThread;
    volatile bool mExit;
};



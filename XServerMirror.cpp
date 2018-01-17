#include <mutex>
#include <X11/extensions/Xfixes.h>

#include "XServerMirror.h"
#include "RenderingEngine.h"
#include "LoadPng.h"

SDL_Event clicknow;
Mirror::Mirror()
    : display{nullptr},
      window{0},
      pos{0, 0, 0, 0},
      rot{0, 0, 0, 1},
      width{0},
      height{0},
      scale{1.0 / 384},
      transparency{0x80},
      haveFocus{false},
      updateInterval{200},
      nextUpdate{std::chrono::system_clock::now() + updateInterval},
      requests{0},
      responses{0},
      toBeDeleted{false},
      worker(&Mirror::thrFnc, this, this),
      era{0},
      mTextWidth{0},
      mTextHeight{0}
{
    std::cout << "new mirror [" << name << "] created\n";
    mCursor = std::make_shared<XFixesCursorImage>();
    int width, height;
    bool alpha;
    GLubyte *textureImage{nullptr};
    if (!loadPngImage("cursor16.png", width, height, alpha, &textureImage))
    {
        logw_ << "No cursor";
        mCursor.reset();
        return;
    }
    mCursor->width = width;
    mCursor->height = height;
    mCursor->pixels = (long unsigned int*)textureImage;
}

Mirror::~Mirror() {
    std::cout << "mirror [" << name << "] about to be destroyed\n";
    toBeDeleted = true;
    requests.post();
    worker.join();
    std::cout << "mirror [" << name << "] destoroyed\n";
}

void prn(XImage* p)
{
    std::cout << "width " << p->width << "\n"
              << "height " << p->height << "\n"
              << "xoffset " << p->xoffset << "\n"
              << "byte_order " << (p->byte_order == LSBFirst ? "LSB" : "MSB")
              << "\n"
              << "bitmap_unit " << p->bitmap_unit << "\n"
              << "bitmap_bit_order "
              << (p->bitmap_bit_order == LSBFirst ? "LSB" : "MSB") << "\n"
              << "bitmap_pad " << p->bitmap_pad << "\n"
              << "depth " << p->depth << "\n"
              << "bytes_per_line " << p->bytes_per_line << "\n"
              << "bits_per_pixel " << p->bits_per_pixel << "\n"
              << "red_mask " << std::hex << p->red_mask << "\n"
              << "green_mask " << std::hex << p->green_mask << "\n"
              << "blue_mask " << std::hex << p->blue_mask << "\n"
              << std::dec;
}

void Mirror::burnMousePointer(Display* display, Window window, XWindowAttributes gwa)
{
    Window focus_return;
    int revert_to_return;
    XGetInputFocus(display, &focus_return, &revert_to_return);
    if (focus_return == window)
    {
        Window root_return, child_return;
        int root_x_return, root_y_return;
        int win_x_return, win_y_return;
        unsigned int mask_return;
        if (XQueryPointer(display,
                          window,
                          &root_return,
                          &child_return,
                          &root_x_return, &root_y_return,
                          &win_x_return, &win_y_return,
                         &mask_return))
        {
            if (win_x_return >=0 && win_x_return < gwa.width &&
                win_y_return >=0 && win_y_return < gwa.height)
            {
                if (mCursor != nullptr)
                {
                    logd_ << "mouse pos " <<  win_x_return << " " << win_y_return << " " << mCursor->width << " " << mCursor->height << "\n";
                    
                    for (auto ix = 0; ix < mCursor->width; ++ix)
                    {
                        for(auto iy = 0; iy < mCursor->height; ++iy)
                        {
                            auto pixel = reinterpret_cast<uint32_t*>(mCursor->pixels) + (mCursor->height - iy) * mCursor->width + ix;
                            auto out = reinterpret_cast<uint32_t*>(mImage.data());
                            auto op = out + (win_y_return + iy) * gwa.width + (win_x_return + ix);
                            if (*pixel >> 24)
                            {
                                *op = *pixel;
                            }
                        }
                    }
                    
                    /*if (clicknow.type == SDL_MOUSEBUTTONDOWN)
                    {
                        XEvent event;
                        memset(&event, 0x00, sizeof(event));
                        event.type = ButtonPress;
                        event.xbutton.button = Button1;//clicknow.button.button
                        event.xbutton.same_screen = True;
                        event.xbutton.root = DefaultRootWindow(display);
                        event.xbutton.window = window;
                        event.xbutton.subwindow = 0;
                        event.xbutton.x_root = root_x_return;
                        event.xbutton.y_root = root_y_return;
                        event.xbutton.x = win_x_return;
                        event.xbutton.y = win_y_return;
                        event.xbutton.state = 0;
                        if (XSendEvent(display, PointerWindow, True, ButtonPressMask, &event) == 0)
                        {
                            loge_ << "failed to press button\n";
                        }else{
                            loge_ << "pressed mouse button";
                        }
                    }else if (clicknow.type == SDL_MOUSEBUTTONUP)
                    {
                        XEvent event;
                        memset(&event, 0x00, sizeof(event));
                        event.type = ButtonRelease;
                        event.xbutton.button = Button1;
                        event.xbutton.same_screen = True;
                        event.xbutton.root = DefaultRootWindow(display);
                        event.xbutton.window = window;
                        event.xbutton.subwindow = 0;
                        event.xbutton.x_root = root_x_return;
                        event.xbutton.y_root = root_y_return;
                        event.xbutton.x = win_x_return;
                        event.xbutton.y = win_y_return;
                        event.xbutton.state = Button1Mask;
                        
                        if (XSendEvent(display, PointerWindow, True, ButtonReleaseMask, &event) == 0)
                        {
                            loge_ << "failed to release button\n";
                        }else{
                            loge_ << "released mouse button\n";
                        }
                    }*/
                    clicknow.type = 0;
                }
            }
        }
    }
}

void* Mirror::thrFnc(Mirror* me)
{
    static std::mutex mtx;
    logi_ << "worker entered [" << me->name << "]\n";
    for (; !me->toBeDeleted;)
    {
        me->requests.wait();
        if (toBeDeleted)
        {
            return nullptr;
        }
        mtx.lock();
        logi_ << "worker serving request [" << me->name << "], display " << me->display << " window id " << me->window << "\n";
        mtx.unlock();
        XImage* image{nullptr};
        XWindowAttributes gwa;
        if (me->display == nullptr || me->window == 0)
        {
            mtx.lock();
            logw_ << "worker serving request failed [" << me->name << "], display " << me->display << " window id " << me->window << "\n";
            mtx.unlock();
        }
        else
        {
            XGetWindowAttributes(me->display, me->window, &gwa);

            image = XGetImage(me->display, me->window, 0, 0, gwa.width,
                              gwa.height, AllPlanes, ZPixmap);
        }
        /*mtx.lock();
        prn(image);
        mtx.unlock();*/
        if (image == nullptr) {
            mtx.lock();
            logw_ << "worker serving request null [" << me->name << "], display " << me->display << " window id " << me->window << "\n";
            mtx.unlock();
        }
        else
        {
            // let's say we support only 32 bpp
            if (image->bits_per_pixel == 32)
            {
                auto outImageSize = image->height * image->width * 4u;//RGB888 //image->bytes_per_line * image->height;
               
                if (mImage.size() < outImageSize)
                {
                    mImage.resize(outImageSize);
                }
                
                me->width = image->width;
                me->height = image->height;

                for (auto row = 0; row < image->height; ++row)
                {
                    auto rowAdr = row * image->bytes_per_line;
                    for (auto col = 0; col < image->width; ++col)
                    {
                        auto InPixelAdr = rowAdr + (col * image->bits_per_pixel) / 8;
                        auto InPixelValue = *reinterpret_cast<uint32_t*>(image->data + InPixelAdr);
                        auto OutPixelAdr = mImage.data() + (row * image->width + col) * 4;
                        *(OutPixelAdr + 3) = 128;
                        *(OutPixelAdr + 2) = (InPixelValue & image->red_mask) >> 16;
                        *(OutPixelAdr + 1) = (InPixelValue & image->green_mask) >> 8;
                        *(OutPixelAdr + 0) = (InPixelValue & image->blue_mask) >> 0;
                    }
                }
                burnMousePointer(me->display, me->window, gwa);
            }
            else
            {
                mtx.lock();
                logw_ << "worker serving request !32bps [" << me->name << "], display " << me->display << " window id " << me->window << "\n";
                mtx.unlock();
           
            }
            XDestroyImage(image);
        }

        me->responses.post();
    }

    logi_ << "worker exited [" << me->name << "]\n";
    return nullptr;
}

std::chrono::system_clock::time_point XServerMirror::findSleepTime()
{
    if (mMasterList.empty()) {
        return std::chrono::system_clock::now() + std::chrono::milliseconds(50);
    }

    auto next = mMasterList.front()->nextUpdate;
    for (auto& i : mMasterList) {
        if (i->nextUpdate < next) {
            next = i->nextUpdate;
        }
    }
    
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(next - std::chrono::system_clock::now());
    if (diff <= std::chrono::milliseconds(0))
    {
        logw_ << "System is too slow, was supposed to render " << diff.count() << " ms!\n";
    }else
    {
        logi_ << "System will sleep :) " << diff.count() << " ms!\n";
    }
    
    return next;
}

void XServerMirror::UpdateMasterList(Display* display, Window win) {
    Atom a = XInternAtom(display, "_NET_CLIENT_LIST", true);
    Atom actualType;
    int format;
    unsigned long numItems, bytesAfter;
    unsigned char* data = 0;
    int status =
        XGetWindowProperty(display, win, a, 0L, (~0L), false, AnyPropertyType,
                           &actualType, &format, &numItems, &bytesAfter, &data);

    if (status >= Success && numItems)
    {
        long* array = (long*)data;
        for (unsigned long k = 0; k < numItems; k++)
        {
            // get window Id:
            Window w = (Window)array[k];

            char* tmp = '\0';
            status = XFetchName(display, w, &tmp);
            std::string name = std::string("noname_") + std::to_string(w);
            if (status >= Success && tmp != nullptr)
            {
                name = tmp;
                XFree(tmp);
            }
            
            logd_ << "window id " << w << " [" << name << "]\n";

            if (find_if(mBlackList.begin(),
                        mBlackList.end(),
                        [&](auto& mirror)
                        {
                            return mirror->window == w;
                        }) != mBlackList.end())
            {
                logd_ << "black listed\n";
                continue;
            }
            // not black listed
            auto mirror = find_if(
                                  mMasterList.begin(),
                                  mMasterList.end(),
                                  [&](auto& mirror)
                                  {
                                    return mirror->window == w;
                                  });
            if (mirror == mMasterList.end())
            {
                // not on master list ->
                // add, must be newly
                // opened
                mMasterList.push_back(std::make_shared<Mirror>());
                mMasterList.back()->name = name;
                mMasterList.back()->display = display;
                mMasterList.back()->window = w;
                mMasterList.back()->era = mEra;
                logd_ << "new one\n";
            } else
            {  // update existing mirror
                (*mirror)->display = display;
                (*mirror)->era = mEra;
                (*mirror)->name = name;
                logd_ << "updated\n";
            }
        }
        XFree(data);

        mMasterList.remove_if([&](auto& mirror) { return mirror->era != mEra; });

        ++mEra;
    }
    else
    {
        loge_ << "Failed to get property from root window\n";
    }
}

boost::property_tree::ptree XServerMirror::serializeList(
    const std::list<std::shared_ptr<Mirror>>& list)
{
    boost::property_tree::ptree temp;
    boost::property_tree::ptree jsonMirrors;
    for (auto& mirror : list)
    {
        boost::property_tree::ptree jsonMirror;
        jsonMirror.put("name", mirror->name);
        
        jsonMirror.put("posx", mirror->pos.x);
        jsonMirror.put("posy", mirror->pos.y);
        jsonMirror.put("posz", mirror->pos.z);
        
        jsonMirror.put("rot1", mirror->rot.x);
        jsonMirror.put("rot2", mirror->rot.y);
        jsonMirror.put("rot3", mirror->rot.z);
        jsonMirror.put("rot4", mirror->rot.w);
        
        jsonMirror.put("scale", mirror->scale);
        
        jsonMirror.put("transparency", mirror->transparency);
        
        jsonMirror.put("updateInterval", mirror->updateInterval.count());

        jsonMirrors.push_back(std::make_pair("", jsonMirror));
    }
    
    temp.add_child("Mirrors", jsonMirrors);
    
    return temp;
}

std::experimental::optional<Window> XServerMirror::getWindowIdFromName(const std::string& name)
{
    Atom a = XInternAtom(mDisplay, "_NET_CLIENT_LIST", true);
    Atom actualType;
    int format;
    unsigned long numItems, bytesAfter;
    unsigned char* data = 0;
    int status =
        XGetWindowProperty(mDisplay, mRootWindow, a, 0L, (~0L), false, AnyPropertyType,
                           &actualType, &format, &numItems, &bytesAfter, &data);

    if (status >= Success && numItems)
    {
        long* array = (long*)data;
        for (unsigned long k = 0; k < numItems; k++)
        {
            // get window Id:
            Window w = (Window)array[k];

            char* winName = '\0';
            status = XFetchName(mDisplay, w, &winName);
            if (status >= Success && winName != nullptr)
            {
                if (winName == nullptr)
                {
                    continue;
                }
                if (name == winName)
                {
                    XFree(winName);
                    return w;
                }
                XFree(winName);
            }
        }
        XFree(data);
    }

    return std::experimental::optional<Window>();
}

std::list<std::shared_ptr<Mirror>> XServerMirror::deSerializeList(
    const boost::property_tree::ptree& tree)
{
    std::list<std::shared_ptr<Mirror>> temp;
    for (auto& mirror : tree.get_child("Mirrors"))
    {
        auto m = std::make_shared<Mirror>();
        m->name = mirror.second.get<std::string>("name");
        auto winId = getWindowIdFromName(m->name);
        if (!winId)
        {
            continue;
        }
        m->window = *winId;
        m->pos.x = mirror.second.get<float>("posx");
        m->pos.y = mirror.second.get<float>("posy");
        m->pos.z = mirror.second.get<float>("posz");
        m->pos.w = 1.0f;//valid
        m->rot.x = mirror.second.get<float>("rot1");
        m->rot.y = mirror.second.get<float>("rot2");
        m->rot.z = mirror.second.get<float>("rot3");
        m->rot.w = mirror.second.get<float>("rot4");
        m->scale = mirror.second.get<float>("scale");
        m->transparency = mirror.second.get<uint8_t>("transparency");
        m->updateInterval = std::chrono::milliseconds(mirror.second.get<int>("updateInterval"));
        temp.push_back(m);
    }
    (void)tree;
    return temp;

}

void XServerMirror::generateHud(RenderingEngine* renderingEngine, cl_float x, cl_float y)
{
    char text[128];
    snprintf(text, sizeof(text),
             "Pos: %2.1f %2.1f %2.1f - [%s] %zd %zd",
             t, u, v, mMirrorWithFocus ? mMirrorWithFocus->name.c_str() : "---", (mCounters["cpy"]), (mCounters["updt"]));
    renderingEngine->draw_text(x, y - 0.03, 0, 0.00015, text, true);
}

void XServerMirror::handleEvents(SDL_Event& event)
{
    if (event.type == SDL_MOUSEBUTTONDOWN ||
        event.type == SDL_MOUSEBUTTONUP)
    {
        clicknow = event;
    }
    
    if (event.type != SDL_KEYDOWN)
    {
        return;
    }
    switch (event.key.keysym.sym)
    {
        case SDLK_f:
            mRenderedItems["dragmode"] = !mRenderedItems["dragmode"];
            break;
        case SDLK_b:
            if (mMirrorWithFocus)
            {
                mBlackList.push_back(mMirrorWithFocus);
            }
            break;
        case SDLK_PAGEUP:
            if (mRenderedItems["dragmode"] && mMirrorWithFocus)
            {
                mMirrorWithFocus->scale *= 1.1;
            }
            break;
         case SDLK_PAGEDOWN:
            if (mRenderedItems["dragmode"] && mMirrorWithFocus)
            {
                if (mMirrorWithFocus->scale > 0)
                {
                    mMirrorWithFocus->scale *= 0.9;
                }
            }
            break;
    }
}

int handlerX11(Display * d, XErrorEvent * e);
XServerMirror::XServerMirror(const std::string& masterListName,
                             const std::string& blackListName)
    : mMasterListName{masterListName},
      mBlackListName{blackListName},
      mEra{1},
      mDisplay{nullptr},
      mRootWindow{0},
      mRequestSceneGeneration{0}
{
    XInitThreads();

    mDisplay = XOpenDisplay(":0.0");
    XSetErrorHandler(handlerX11);
    mRootWindow = DefaultRootWindow(mDisplay);

    try
    {
        boost::property_tree::ptree tree;
        read_json(mMasterListName, tree);
        mMasterList = deSerializeList(tree);
            
        read_json(mBlackListName, tree);
        mBlackList = deSerializeList(tree);
    } catch (...)
    {
        logw_ << "Master/Black list not found\n";
    }
       
    mRenderedItems["dragmode"] = false;
}

int handlerX11(Display * d, XErrorEvent * e)
{
    (void)d;
    loge_ << "Error code: " << e->error_code << std::endl;
    return 0;
}        

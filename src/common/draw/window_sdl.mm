#include "window_sdl.h"

#ifdef __APPLE__
#include "window_metal.h"
#else
#include "window_gl.h"
#endif

using namespace Eigen;

Vector<Tuint, 2> sdl_window::get_size() const
{
    int width;
    int height;
    SDL_GetWindowSize(window, &width, &height);
    return { width, height };
}

void sdl_window::draw(std::function<void(Vector<Tuint, 2>, Tuint* p)> func)
{
    Vector<Tuint, 2> size = get_size();
    if (size != surface_size)
    {
        if (surface != nullptr)
        {
            SDL_FreeSurface(surface);
            surface = nullptr;
        }
        surface = nullptr;
        surface = SDL_GetWindowSurface(window);
        if (surface == nullptr)
        {
            throw std::runtime_error(std::string("Cannot create surface: ") + SDL_GetError());
        }
        surface_size = size;
    }
    if (SDL_LockSurface(surface) != 0)
    {
        throw std::runtime_error("SDL_LockSurface failed");
    }
    if constexpr (goopax::is_debugtype<Tuint>::value)
    {
        std::vector<Tuint> tmp(size[0] * size[1]);
        func(size, tmp.data());
        std::copy(tmp.begin(), tmp.end(), reinterpret_cast<uint32_t*>(surface->pixels));
    }
    else
    {
        func(size, reinterpret_cast<Tuint32_t*>(surface->pixels));
    }
    SDL_UnlockSurface(surface);
    SDL_UpdateWindowSurface(window);
}

std::optional<SDL_Event> sdl_window::get_event()
{
    SDL_Event e;
    if (SDL_PollEvent(&e) != 0)
    {
        return e;
    }
    else
    {
        return {};
    }
}

std::optional<SDL_Event> sdl_window::wait_event()
{
    SDL_Event e;
    if (SDL_WaitEvent(&e))
    {
        return e;
    }
    else
    {
        throw std::runtime_error(SDL_GetError());
    }
}

void sdl_window::set_title(const std::string& title) const
{
    SDL_SetWindowTitle(window, title.c_str());
}

std::unique_ptr<sdl_window> sdl_window::create(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags)
{
#ifdef __APPLE__
    return create_sdl_window_metal(name, size, flags);
#else
    return std::make_unique<sdl_window_gl>(name, size, flags);
#endif
}

sdl_window::sdl_window(const char* name, Vector<Tuint, 2> size, uint32_t flags)
{
    static std::once_flag once;
    call_once(once, []() {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            throw std::runtime_error(SDL_GetError());
        }
    });

    std::atexit([]() { SDL_Quit(); });

    window = SDL_CreateWindow(name,                    // window title
                              SDL_WINDOWPOS_UNDEFINED, // initial x position
                              SDL_WINDOWPOS_UNDEFINED, // initial y position
                              size[0],                 // width, in pixels
                              size[1],                 // height, in pixels
                              flags                    // SDL_WINDOW_OPENGL                  // flags - see below
    );
    if (window == nullptr)
    {
        throw std::runtime_error(std::string("Cannot create window: ") + SDL_GetError());
    }
}

sdl_window::~sdl_window()
{
    if (surface != nullptr)
    {
        SDL_FreeSurface(surface);
        surface = nullptr;
    }
    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}

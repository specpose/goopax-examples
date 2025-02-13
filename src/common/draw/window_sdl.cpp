#include "window_sdl.h"
#include "window_gl.h"
#include "window_plain.h"

#include <goopax>

using namespace Eigen;
using namespace goopax;
using namespace std;

#if !USE_SDL2
void print_properties(unsigned int props)
{
    SDL_EnumerateProperties(
        props,
        [](void* userdata, SDL_PropertiesID props, const char* name) {
            cout << name << ": ";

            auto type = SDL_GetPropertyType(props, name);

            if (type == SDL_PROPERTY_TYPE_POINTER)
            {
                cout << SDL_GetPointerProperty(props, name, nullptr);
            }
            else if (type == SDL_PROPERTY_TYPE_STRING)
            {
                cout << SDL_GetStringProperty(props, name, "");
            }
            else if (type == SDL_PROPERTY_TYPE_NUMBER)
            {
                cout << SDL_GetNumberProperty(props, name, -1);
            }
            else if (type == SDL_PROPERTY_TYPE_FLOAT)
            {
                cout << SDL_GetFloatProperty(props, name, numeric_limits<float>::quiet_NaN());
            }
            else if (type == SDL_PROPERTY_TYPE_BOOLEAN)
            {
                cout << SDL_GetBooleanProperty(props, name, false);
            }
            else
            {
                cout << "BAD TYPE";
            }
            cout << endl;
        },
        nullptr);
}
#endif

std::array<unsigned int, 2> sdl_window::get_size() const
{
    int width;
    int height;
    SDL_GetWindowSize(window, &width, &height);
    return { (unsigned int)width, (unsigned int)height };
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
#if WITH_METAL
    try
    {
        cout << "Trying metal." << endl;
        return create_sdl_window_metal(name, size, flags);
    }
    catch (std::exception& e)
    {
        cout << "Got exception '" << e.what() << "'" << endl;
    }
#endif
#if WITH_OPENGL
    try
    {
        cout << "Trying opengl." << endl;
        return std::make_unique<sdl_window_gl>(name, size, flags);
    }
    catch (std::exception& e)
    {
        cout << "Got exception '" << e.what() << "'" << endl;
    }
#endif
    try
    {
        cout << "Trying plain." << endl;
        return std::make_unique<sdl_window_plain>(name, size, flags);
    }
    catch (std::exception& e)
    {
        cout << "Got exception '" << e.what() << "'" << endl;
    }

    throw std::runtime_error("Failed to open window");
}

sdl_window::sdl_window(const char* name, Vector<Tuint, 2> size, uint32_t flags, const char* renderer_name)
{
    static std::once_flag once;
    call_once(once, []() {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            throw std::runtime_error(SDL_GetError());
        }
    });

    std::atexit([]() { SDL_Quit(); });

    window = SDL_CreateWindow(name,    // window title
#if USE_SDL2
                              SDL_WINDOWPOS_UNDEFINED, // initial x position
                              SDL_WINDOWPOS_UNDEFINED, // initial y position
#endif
                              size[0], // width, in pixels
                              size[1], // height, in pixels
                              flags    /*| SDL_WINDOW_OPENGL */
    );
    if (window == nullptr)
    {
        throw std::runtime_error(std::string("Cannot create window: ") + SDL_GetError());
    }

    if (renderer_name != nullptr)
    {
#if !USE_SDL2
        SDL_PropertiesID props = SDL_CreateProperties();

        bool ok = true;
        ok = ok && SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, renderer_name);
        ok = ok && SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
        assert(ok);

        renderer = SDL_CreateRendererWithProperties(props);

        SDL_DestroyProperties(props);

        if (renderer == nullptr)
        {
            SDL_DestroyWindow(window);
            window = nullptr;
            throw std::runtime_error(std::string("Cannot create renderer: ") + SDL_GetError());
        }

        cout << "renderer properties:" << endl;
        print_properties(SDL_GetRendererProperties(renderer));
#else
        renderer = SDL_CreateRenderer(window, -1, 0);
#endif
    }
}

sdl_window::~sdl_window()
{
    if (texture != nullptr)
    {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (renderer != nullptr)
    {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}

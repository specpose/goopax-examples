#include "window_sdl.h"
#include "window_gl.h"
#include "window_plain.h"
#include "window_vulkan.h"

#include <goopax>

using namespace Eigen;
using namespace goopax;
using namespace std;

void print_properties(unsigned int props)
{
    SDL_EnumerateProperties(
        props,
        [](void*, SDL_PropertiesID props, const char* name) {
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
    call_sdl(SDL_WaitEvent(&e));
    return e;
}

void sdl_window::set_title(const std::string& title) const
{
    SDL_SetWindowTitle(window, title.c_str());
}

void sdl_window::toggle_fullscreen()
{
    if (SDL_SetWindowFullscreen(window, !is_fullscreen))
    {
        is_fullscreen = !is_fullscreen;
    }
    else
    {
        cerr << "Fullscreen failed: " << SDL_GetError() << endl;
    }
}

std::unique_ptr<sdl_window>
sdl_window::create(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags, goopax::envmode env)
{
#if WITH_METAL
    try
    {
        cout << "Trying metal." << endl;
        return create_sdl_window_metal(name, size, flags, env);
    }
    catch (std::exception& e)
    {
        cout << "Got exception '" << e.what() << "'" << endl;
    }
#endif
#if WITH_VULKAN
    if (env & env_VULKAN)
    {
        try
        {
            cout << "Trying vulkan." << endl;
            return std::make_unique<sdl_window_vulkan>(name, size, flags);
        }
        catch (std::exception& e)
        {
            cout << "Got exception '" << e.what() << "'" << endl;
        }
    }
#endif
#if WITH_OPENGL
    try
    {
        cout << "Trying opengl." << endl;
        return std::make_unique<sdl_window_gl>(name, size, flags, env);
    }
    catch (std::exception& e)
    {
        cout << "Got exception '" << e.what() << "'" << endl;
    }
#endif
    try
    {
        cout << "Trying plain." << endl;
        return std::make_unique<sdl_window_plain>(name, size, flags, env);
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
    call_once(once, []() { call_sdl(SDL_Init(SDL_INIT_VIDEO)); });

    std::atexit([]() { SDL_Quit(); });

    window = SDL_CreateWindow(name,    // window title
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

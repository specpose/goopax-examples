#include "window_sdl.h"

#ifdef __linux__
#include <GL/glx.h>
#endif
#include <goopax_gl>

using namespace Eigen;
using namespace goopax;
using namespace std;

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

void sdl_window::draw_goopax_impl(std::function<void(image_buffer<2, Eigen::Vector<uint8_t, 4>, true>& image)> func)
{
    Eigen::Vector<Tuint, 2> size = get_size();

    if (size != texture_size)
    {
        // Either the first call, or the window size has changed. Re-allocating buffers.
        if (texture != nullptr)
        {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, size[0], size[1]);
        if (texture == nullptr)
        {
            throw std::runtime_error(std::string("Cannot create texture: ") + SDL_GetError());
        }

        texture_size = size;

        SDL_PropertiesID texture_props = SDL_GetTextureProperties(texture);

        cout << "\ntexture_props:" << endl;
        print_properties(texture_props);

        string renderer_name =
            SDL_GetStringProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_NAME_STRING, "");
        if (renderer_name == "opengl")
        {
            unsigned int gl_id = SDL_GetNumberProperty(texture_props, SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER, 123);
            image = image_buffer<2, Eigen::Vector<uint8_t, 4>, true>::create_from_gl(gl_device, gl_id);
        }
        else
        {
            throw std::runtime_error("Not implemented: renderer " + renderer_name);
        }

        SDL_DestroyProperties(texture_props);
    }

    func(image);
    flush_gl_interop(gl_device);
    SDL_SetRenderTarget(renderer, nullptr);
    bool ret = SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    if (!ret)
    {
        throw std::runtime_error(std::string("SDL_RenderTexture failed: ") + SDL_GetError());
    }

    SDL_RenderPresent(renderer);
}

Vector<Tuint, 2> sdl_window::get_size() const
{
    int width;
    int height;
    SDL_GetWindowSize(window, &width, &height);
    return { width, height };
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

sdl_window::sdl_window(const char* name, Vector<Tuint, 2> size, uint32_t flags)
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
                              size[0], // width, in pixels
                              size[1], // height, in pixels
                              flags    /*| SDL_WINDOW_OPENGL */
    );
    if (window == nullptr)
    {
        throw std::runtime_error(std::string("Cannot create window: ") + SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
        throw std::runtime_error(std::string("Cannot create renderer: ") + SDL_GetError());
    }

    cout << "renderer properties:" << endl;
    print_properties(SDL_GetRendererProperties(renderer));

    string renderer_name =
        SDL_GetStringProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_NAME_STRING, "");

    cout << "renderer name: " << renderer_name << endl;

    if (renderer_name == "opengl")
    {
        gl_device = goopax::get_devices_from_gl()[0];
    }
    else
    {
        throw std::runtime_error("Not implemented: renderer " + renderer_name);
    }

#if GOOPAXLIB_DEBUG
    device = default_device(env_CPU);
#else
    device = gl_device;
#endif

    image.assign(gl_device, { 0, 0 });
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

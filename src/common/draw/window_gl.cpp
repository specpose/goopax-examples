#include "window_gl.h"

using namespace std;
using namespace goopax;

void sdl_window_gl::draw_goopax(std::function<void(image_buffer<2, Eigen::Vector<uint8_t, 4>, true>& image)> func)
{
    std::array<unsigned int, 2> size = get_size();

    if (size != image.dimensions())
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

        SDL_PropertiesID texture_props = SDL_GetTextureProperties(texture);

        cout << "\ntexture_props:" << endl;
        print_properties(texture_props);

        string renderer_name =
            SDL_GetStringProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_NAME_STRING, "");
        if (renderer_name == "opengl")
        {
            unsigned int gl_id = SDL_GetNumberProperty(texture_props, SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER, 123);
            image = image_buffer<2, Eigen::Vector<uint8_t, 4>, true>::create_from_gl(device, gl_id);
        }
        else
        {
            throw std::runtime_error("Not implemented: renderer " + renderer_name);
        }

        SDL_DestroyProperties(texture_props);
    }

    if (false)
    {
        // Draw some rectangles for testing.
        SDL_FRect r;
        r.w = 100;
        r.h = 50;

        r.x = rand() % 500;
        r.y = rand() % 500;

        SDL_SetRenderTarget(renderer, texture);
        SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);
        SDL_RenderClear(renderer);
        SDL_RenderRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
        SDL_RenderFillRect(renderer, &r);
    }
    else
    {
        func(image);
        flush_graphics_interop(device);
    }
    SDL_SetRenderTarget(renderer, nullptr);
    call_sdl(SDL_RenderTexture(renderer, texture, nullptr, nullptr));

    SDL_RenderPresent(renderer);
}

sdl_window_gl::sdl_window_gl(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags, goopax::envmode env)
    : sdl_window(name, size, flags | SDL_WINDOW_OPENGL, "opengl")
{
    auto devices = goopax::get_devices_from_gl(env);
    if (devices.empty())
    {
        throw std::runtime_error("Cannot create goopax device for opengl");
    }

    this->image.assign(devices[0], { 0, 0 });
    cout << "devices.size()=" << devices.size() << endl;
    this->device = devices[0];
    cout << "Using device " << this->device.name() << ", env=" << this->device.get_envmode() << endl;
}

#include "window_metal.h"
#include <SDL3/SDL_render.h>

std::unique_ptr<sdl_window> create_sdl_window_metal(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags)
{
    return std::make_unique<sdl_window_metal>(name, size, flags);
}

void sdl_window_metal::draw_goopax_impl(
    std::function<void(goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true>& image)> func)
{
    @autoreleasepool
    {
        id<CAMetalDrawable> surface = [this->swapchain nextDrawable];
        if (surface == nullptr)
        {
            throw std::runtime_error("Failure getting nextDrawable");
        }
        id<MTLCommandBuffer> buffer = [this->queue commandBuffer];

        auto image =
            goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true>::create_from_metal(device, surface.texture);

        func(image);

        [buffer presentDrawable:surface];
        [buffer commit];
    }
}

void sdl_window_metal::cleanup()
{
}

sdl_window_metal::sdl_window_metal(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags)
    : sdl_window(name, size, flags | SDL_WINDOW_METAL)
{
    try
    {
        renderer = SDL_CreateRenderer(window, -1);
        /*        {
                    SDL_RendererInfo info;
                    int err = SDL_GetRendererInfo(renderer, &info);
                    if (err == 0)
                    {
                        std::cout << "renderer: name=" << info.name << ", max width=" << info.max_texture_width
                                  << ", max height=" << info.max_texture_height << ", flags=" << info.flags <<
           std::endl;
                    }
                    else
                    {
                        std::cerr << "Got some error while calling SDL_GetRendererInfo." << std::endl;
                    }
                }
         */
        swapchain = (__bridge CAMetalLayer*)SDL_RenderGetMetalLayer(renderer);
        if (swapchain == nullptr)
        {
            throw std::runtime_error("It looks like the Metal renderer is not available in SDL.");
        }
        const id<MTLDevice> gpu = swapchain.device;
        queue = [gpu newCommandQueue];
        SDL_DestroyRenderer(renderer);
    }
    catch (...)
    {
        cleanup();
        throw;
    }
#if GOOPAXLIB_DEBUG
    device = goopax::default_device(goopax::env_CPU);
#else
    device = goopax::default_device();
#endif
}

sdl_window_metal::~sdl_window_metal()
{
    cleanup();
}

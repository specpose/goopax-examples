#include "window_metal.h"
using namespace std;

std::unique_ptr<sdl_window>
sdl_window::create_sdl_window_metal(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags, goopax::envmode env)
{
    return std::make_unique<sdl_window_metal>(name, size, flags);
}

void sdl_window_metal::draw_goopax(
    std::function<void(goopax::image_buffer<2, Eigen::Vector<Tuint8_t, 4>, true>& image)> func)
{
    @autoreleasepool
    {
        id<CAMetalDrawable> surface = [this->swapchain nextDrawable];
        if (surface == nullptr)
        {
            cerr << "Failure getting nextDrawable" << endl;
            return;
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

sdl_window_metal::sdl_window_metal(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags, goopax::envmode env)
    : sdl_window(name, size, flags | SDL_WINDOW_METAL, "metal")
{
    try
    {
        swapchain = (__bridge CAMetalLayer*)SDL_GetRenderMetalLayer(renderer);
        if (swapchain == nullptr)
        {
            throw std::runtime_error("It looks like the Metal renderer is not available in SDL.");
        }
        const id<MTLDevice> gpu = swapchain.device;
        queue = [gpu newCommandQueue];
    }
    catch (...)
    {
        cleanup();
        throw;
    }

    {
        const auto* mtldevice = swapchain.device;
        cout << "mtldevice=" << mtldevice << endl;
        for (auto& device : goopax::devices(static_cast<goopax::envmode>(env & goopax::env_METAL)))
        {
            cout << "have device: " << device.name() << ", ptr=" << device.get_device_ptr() << flush;
            if (device.get_device_ptr() == mtldevice)
            {
                cout << ". Using." << flush;
                this->device = device;
            }
            cout << endl;
        }
    }
}

sdl_window_metal::~sdl_window_metal()
{
    cleanup();
}

#pragma once

#include "window_sdl.h"

std::unique_ptr<sdl_window> create_sdl_window_metal(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags);

#ifdef __OBJC__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <goopax_metal>

struct sdl_window_metal : sdl_window
{
    SDL_Renderer* renderer = nullptr;
    const CAMetalLayer* swapchain = nullptr;
    id<MTLCommandQueue> queue;

protected:
    virtual void draw_goopax_impl(
        std::function<void(goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true>& image)> func) override final;

private:
    void cleanup();

public:
    sdl_window_metal(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags = 0);
    ~sdl_window_metal();
};
#endif

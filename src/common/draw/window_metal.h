#pragma once

#if WITH_METAL

#include "window_sdl.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <goopax>

std::unique_ptr<sdl_window> create_sdl_window_metal(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags);

struct sdl_window_metal : sdl_window
{
    const CAMetalLayer* swapchain = nullptr;
    id<MTLCommandQueue> queue;

    virtual void draw_goopax(
        std::function<void(goopax::image_buffer<2, Eigen::Vector<Tuint8_t, 4>, true>& image)> func) override final;

private:
    void cleanup();

public:
    sdl_window_metal(const char* name,
                     Eigen::Vector<Tuint, 2> size,
                     uint32_t flags = 0,
                     goopax::envmode env = goopax::env_GPU);
    ~sdl_window_metal();
};
#endif

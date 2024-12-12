#pragma once

#include "window_sdl.h"

class sdl_window_plain : public sdl_window
{
    goopax::image_buffer<2, Eigen::Vector<Tuint8_t, 4>, true> image;

    void draw_goopax_impl(
        std::function<void(goopax::image_buffer<2, Eigen::Vector<Tuint8_t, 4>, true>& image)> func) final override;

public:
    sdl_window_plain(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags = 0);
};

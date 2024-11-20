#pragma once

#include "types.h"
#include <SDL3/SDL.h>
#include <optional>

struct common_window
{
    goopax::goopax_device gl_device;
    goopax::goopax_device device;

    virtual Eigen::Vector<Tuint, 2> get_size() const = 0;
    virtual std::optional<SDL_Event> get_event() = 0;
    virtual std::optional<SDL_Event> wait_event() = 0;

protected:
    virtual void
    draw_goopax_impl(std::function<void(goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true>& image)> func);

public:
    void draw_goopax(std::function<void(goopax::image_buffer<2, Eigen::Vector<Tuint8_t, 4>, true>& image)> func);

    virtual ~common_window() = default;
};

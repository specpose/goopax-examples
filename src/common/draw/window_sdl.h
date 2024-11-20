#pragma once

#include "window.h"

struct sdl_window : common_window
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true> image;

    Eigen::Vector<Tuint, 2> texture_size = { 0, 0 };

protected:
    virtual void draw_goopax_impl(
        std::function<void(goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true>& image)> func) override final;

public:
    virtual Eigen::Vector<Tuint, 2> get_size() const final override;
    virtual std::optional<SDL_Event> get_event() final override;
    virtual std::optional<SDL_Event> wait_event() final override;
    void set_title(const std::string& title) const;

    sdl_window(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags = 0);
    virtual ~sdl_window();

    sdl_window(const sdl_window&) = delete;
    sdl_window& operator=(const sdl_window&) = delete;
};

#pragma once

#include "window.h"

struct sdl_window : common_window
{
    SDL_Window* window = nullptr;
    SDL_Surface* surface = nullptr;
    Eigen::Vector<Tuint, 2> surface_size = { 0, 0 };

    virtual Eigen::Vector<Tuint, 2> get_size() const final override;
    virtual void draw(std::function<void(Eigen::Vector<Tuint, 2>, Tuint* p)> func) final override;
    virtual std::optional<SDL_Event> get_event() final override;
    virtual std::optional<SDL_Event> wait_event() final override;
    void set_title(const std::string& title) const;

    static std::unique_ptr<sdl_window> create(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags = 0);
    sdl_window(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags = 0);
    virtual ~sdl_window();

    sdl_window(const sdl_window&) = delete;
    sdl_window& operator=(const sdl_window&) = delete;
};

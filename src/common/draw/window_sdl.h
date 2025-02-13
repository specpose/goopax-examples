#pragma once

#include "types.h"
#if USE_SDL2
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif
#include <optional>

void print_properties(unsigned int props);

struct sdl_window
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;

    goopax::goopax_device device;

public:
    std::array<unsigned int, 2> get_size() const;
    std::optional<SDL_Event> get_event();
    std::optional<SDL_Event> wait_event();
    void set_title(const std::string& title) const;

    virtual void
    draw_goopax(std::function<void(goopax::image_buffer<2, Eigen::Vector<Tuint8_t, 4>, true>& image)> func) = 0;

    static std::unique_ptr<sdl_window> create(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags = 0);

private:
    static std::unique_ptr<sdl_window>
    create_sdl_window_metal(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags);

public:
    sdl_window(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags, const char* renderer_name);
    virtual ~sdl_window();

    sdl_window(const sdl_window&) = delete;
    sdl_window& operator=(const sdl_window&) = delete;
};

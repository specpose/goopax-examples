#pragma once

#include "window_sdl.h"

#ifdef __APPLE__
#if __has_include(<OpenGL/gl.h>)
#define GOOPAXLIB_HAVE_GL 1
#else
#define GOOPAXLIB_HAVE_GL 0
#endif
#else
#define GOOPAXLIB_HAVE_GL 1
#endif

struct gl_object
{
    unsigned int gl_id;
    bool alive = true;

    inline gl_object() = default;
    gl_object(const gl_object&) = delete;
    gl_object& operator=(const gl_object&) = delete;
    inline gl_object(gl_object&& b)
        : gl_id(b.gl_id)
    {
        b.alive = false;
    }
    inline gl_object& operator=(gl_object&& b)
    {
        gl_id = b.gl_id;
        b.alive = false;
        return *this;
    }
};

struct gl_texture : public gl_object
{
    gl_texture(unsigned int width, unsigned int height);
    gl_texture()
    {
        this->alive = false;
    }
    ~gl_texture();

#ifdef _MSC_VER
    gl_texture(gl_texture&& b)
        : gl_object(std::move(b))
    {
    }
    inline gl_texture& operator=(gl_texture&& b)
    {
        gl_object::operator=(std::move(b));
        return *this;
    }
#else
    gl_texture(gl_texture&&) = default;
    inline gl_texture& operator=(gl_texture&&) = default;
#endif
};

struct gl_buffer : public gl_object
{
    gl_buffer(size_t size);
    ~gl_buffer();

#ifdef _MSC_VER
    gl_buffer(gl_buffer&& b)
        : gl_object(std::move(b))
    {
    }
    gl_buffer& operator=(gl_buffer&& b)
    {
        gl_object::operator=(std::move(b));
        return *this;
    }
#else
    gl_buffer(gl_buffer&&) = default;
    gl_buffer& operator=(gl_buffer&&) = default;
#endif
};

template<class T>
struct opengl_buffer
    : public gl_buffer
    , public goopax::buffer<T>
{
    opengl_buffer(goopax::goopax_device device,
                  const size_t size,
                  goopax::BUFFER_FLAGS flags = goopax::BUFFER_READ_WRITE)
        : gl_buffer(size * sizeof(T))
        , goopax::buffer<T>(goopax::buffer<T>::create_from_gl(device, this->gl_buffer::gl_id, flags))
    {
    }

    opengl_buffer(opengl_buffer&&) = default;
    opengl_buffer& operator=(opengl_buffer&&) = default;
};

struct sdl_window_gl : sdl_window
{
    SDL_GLContext context = nullptr;

    gl_texture tex;
    goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true> image;

protected:
    virtual void draw_goopax_impl(
        std::function<void(goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true>& image)> func) override final;

public:
    sdl_window_gl(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags = 0);

    void cleanup()
    {
        if (context != nullptr)
        {
            SDL_GL_DestroyContext(context);
            context = nullptr;
        }
    }

    ~sdl_window_gl()
    {
        cleanup();
    }
};

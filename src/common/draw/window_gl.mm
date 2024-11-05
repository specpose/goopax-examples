#include "window_gl.h"

#if GOOPAXLIB_HAVE_GL

#ifdef __APPLE__
#if __has_include(<OpenGL/gl.h>)
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl.h>
#endif
#else
#ifdef __linux__
#include <GL/glx.h>
#endif
#include <glatter/glatter.h>
#endif
#include <goopax_gl>

using namespace goopax;

gl_texture::gl_texture(unsigned int width, unsigned int height)
{
    // Create texture object
    glGenTextures(1, &gl_id);
    glBindTexture(GL_TEXTURE_2D, gl_id);

    // Set parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

gl_texture::~gl_texture()
{
    if (alive)
        glDeleteTextures(1, &gl_id);
}

gl_buffer::gl_buffer(size_t size)
{
    glGenBuffers(1, &gl_id);
    glBindBuffer(GL_ARRAY_BUFFER, gl_id);

    // buffer data
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

gl_buffer::~gl_buffer()
{
    if (alive)
        glDeleteBuffers(1, &gl_id);
}

void sdl_window_gl::draw_goopax_impl(std::function<void(image_buffer<2, Eigen::Vector<uint8_t, 4>, true>& image)> func)
{
    auto dim = get_size();
    std::array<unsigned int, 2> imagedim = image.dimensions();
    if (dim[0] != imagedim[0] || dim[1] != imagedim[1])
    {
        tex = gl_texture(dim[0], dim[1]);
        image = image_buffer<2, Eigen::Vector<uint8_t, 4>, true>::create_from_gl(gl_device, tex.gl_id);
    }

    func(image);

    flush_gl_interop(gl_device);

    glBindTexture(GL_TEXTURE_2D, tex.gl_id);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.0, 1.0, 1.0, -1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glViewport(0, 0, dim[0], dim[1]);

    glBegin(GL_QUADS);

    glTexCoord2f(0.0, 0.0);
    glVertex2f(-1.0, -1.0);

    glTexCoord2f(1.0, 0.0);
    glVertex2f(1.0, -1.0);

    glTexCoord2f(1.0, 1.0);
    glVertex2f(1.0, 1.0);

    glTexCoord2f(0.0, 1.0);
    glVertex2f(-1.0, 1.0);

    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);

    SDL_GL_SwapWindow(window);
}

sdl_window_gl::sdl_window_gl(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags)
    : sdl_window(name, size, flags | SDL_WINDOW_OPENGL)
{
    context = SDL_GL_CreateContext(window);
    if (context == nullptr)
    {
        std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
        throw std::runtime_error("cannot create OpenGL context");
    }
    gl_device = get_devices_from_gl()[0];
#if GOOPAXLIB_DEBUG
    device = default_device(env_CPU);
#else
    device = gl_device;
#endif

    image.assign(gl_device, { 0, 0 });
}

#endif

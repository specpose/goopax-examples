#include "window.h"

using namespace goopax;
using namespace Eigen;

void common_window::draw_goopax(std::function<void(image_buffer<2, Vector<Tuint8_t, 4>, true>& image)> func)
{
#if GOOPAXLIB_DEBUG
    draw_goopax_impl([&func, this](image_buffer<2, Vector<uint8_t, 4>, true>& image_gl) {
        image_buffer<2, Vector<Tuint8_t, 4>, true> image(device, image_gl.dimensions());
        func(image);
        std::vector<Vector<uint8_t, 4>> tmp(image.dimensions()[0] * image.dimensions()[1]);
        const_image_buffer_map map(image);
        image_buffer_map map_gl(image_gl, BUFFER_WRITE_DISCARD);
        for (uint y = 0; y < image.dimensions()[1]; ++y)
        {
            for (uint x = 0; x < image.dimensions()[0]; ++x)
            {
                map_gl[{ x, y }] = map[{ x, y }].cast<uint8_t>();
            }
        }
    });
#else
    draw_goopax_impl(func);
#endif
}

void common_window::draw_goopax_impl(std::function<void(goopax::image_buffer<2, Vector<uint8_t, 4>, true>& image)> func)
{
    (void)func;
    goopax::interface::warning() << "draw_goopax_impl not implemented.";
}

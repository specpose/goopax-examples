// @@@ CONVERT_TYPES_IGNORE @@@
#include <goopax_gl.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <complex> // Using the complex numbers from the STL library.
using namespace goopax;
using std::complex;

auto mandelbrot = make_kernel([](resource<unsigned int>& image,
                                 const gpu_uint width,
                                 const gpu_uint height,
                                 const complex<gpu_float> center,
                                 const gpu_float scale) {
    gpu_for_global(gpu_uint k = 0, image.size())
    {
        const gpu_uint ix = k % width;
        const gpu_uint iy = k / width;

        const complex<gpu_float> pos = { ix - width * 0.5f, iy - height * 0.5f };

        const complex<gpu_float> c = center + scale * pos;
        complex<gpu_float> z = { 0, 0 };

        gpu_for(gpu_uint i = 0, 256)
        {
            z = z * z + c;
        }
        image[k] = cond(norm(z) < 4.0, 0x00000000u, 0x00ffffffu);
    }
});

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitWindowSize(1024, 768);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutCreateWindow("Mandelbrot");
#ifndef __APPLE__
    glewInit();
#endif

    static const int width = 1024;
    static const int height = 768;

    goopax_env_gl env(argc, argv); // Initialize GOOPAX for use with OpenGL

    complex<float> center = { -1, 0 };
    float scale = 3E-3;
    float speed_zoom = -1E-3;

    gl_texture tex(width, height);
    opengl_buffer<unsigned int> image(width * height); // Allocate image buffer for OpenGL
    while (true)
    {
        scale *= exp(speed_zoom);
        mandelbrot(image, width, height, center, scale); // Call the kernel
        draw_bitmap(image, tex, width, height);
        glutSwapBuffers();
    }
}

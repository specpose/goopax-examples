/**
   \example nbody.cpp
   Simple N-body example program
 */

#include "common/draw/window_sdl.h"
#include "common/particle.hpp"
#include <SDL3/SDL_main.h>
#include <chrono>
#include <random>
using std::chrono::steady_clock;

using namespace Eigen;
using namespace goopax;
using namespace std;

void init(buffer<Vector3<float>>& x, buffer<Vector3<float>>& v)
{
    default_random_engine generator;
    normal_distribution<float> distribution;

    vector<Vector3<float>> vmap(v.size());
    vector<Vector3<float>> xmap(x.size());
    for (Tuint k = 0; k < x.size(); ++k) // Setting the initial conditions:
    {                                    // N particles of mass 1/N each are randomly placed in a sphere of radius 1
        Vector3<float> xk;
        Vector3<float> vk;
        do
        {
            for (Tuint i = 0; i < 3; ++i)
            {
                xk[i] = distribution(generator) * 0.2;
                vk[i] = distribution(generator) * 0.2;
            }
        } while (xk.squaredNorm() >= 1);
        vk += Vector3<float>({ -xk[1], xk[0], 0 }) / Vector3<float>({ -xk[1], xk[0], 0 }).norm() * 0.4
              * min(xk.norm() * 10, 1.f);
        if (k < x.size() / 2)
            vk = -vk;
        if (k < x.size() / 2)
        {
            xk += Vector3<float>({ 0.8f, 0.2f, 0.0f });
            vk += Vector3<float>({ -0.4f, 0.0f, 0.0f });
        }
        else
        {
            xk -= Vector3<float>({ 0.8f, 0.2f, 0.0f });
            vk += Vector3<float>({ 0.4f, 0.0f, 0.0f });
        }
        vmap[k] = vk;
        xmap[k] = xk;
    }
    v = std::move(vmap);
    x = std::move(xmap);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    const int N = 65536;
    const double dt = 5E-3;

    unique_ptr<sdl_window> window =
        sdl_window::create("nbody", { 1024, 768 }, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    goopax_device device = window->device;

#if WITH_METAL
    particle_renderer Renderer(dynamic_cast<sdl_window_metal&>(*window));
    buffer<Vector3<float>> x(device, N);  // OpenGL buffer
    buffer<Vector3<float>> x2(device, N); // OpenGL buffer
#else
    opengl_buffer<Vector3<float>> x(device, N);  // OpenGL buffer
    opengl_buffer<Vector3<float>> x2(device, N); // OpenGL buffer
#endif

    kernel CalculateForce(
        device,
        [](const resource<Vector3<float>>& x,
           resource<Vector3<float>>& v,
           gpu_float dt,
           gpu_float mass,
           resource<Vector3<float>>& xnew) {
            Vector3<gpu_float> F = { 0, 0, 0 };
            const gpu_uint i = global_id();

            gpu_for(0, x.size(), [&](gpu_uint k) {
                Vector3<gpu_float> r = x[k] - x[i];
                F += r * pow<-3, 2>(r.squaredNorm() + 1E-20f);
            });

            v[i] += F * (dt * mass);
            xnew[i] = x[i] + v[i] * dt;
        },
        0,
        N);

    buffer<Vector3<float>> v(device, N); // and the velocities.
    init(x, v);

    bool quit = false;
    bool is_fullscreen = false;
    while (!quit)
    {
        while (auto e = window->get_event())
        {
            if (e->type == SDL_EVENT_QUIT)
            {
                quit = true;
            }
            else if (e->type == SDL_EVENT_KEY_DOWN)
            {
                switch (e->key.key)
                {
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                    case SDLK_F: {
                        if (SDL_SetWindowFullscreen(window->window, !is_fullscreen))
                        {
                            is_fullscreen = !is_fullscreen;
                        }
                        else
                        {
                            cerr << "Fullscreen failed: " << SDL_GetError() << endl;
                        }
                    }
                    break;
                };
            }
        }

        static auto frametime = steady_clock::now();
        static Tint framecount = 0;

        goopax_future f = CalculateForce(x, v, dt, 1.0 / x.size(), x2);
        swap(x, x2);

        auto now = steady_clock::now();
        ++framecount;
        if (now - frametime > chrono::seconds(1))
        {
            stringstream title;
            Tdouble rate = framecount / chrono::duration<double>(now - frametime).count();
            title << "N-body. N=" << N << ", " << rate << " fps, device=" << device.name();
            string s = title.str();
            SDL_SetWindowTitle(window->window, s.c_str());
            framecount = 0;
            frametime = now;
        }

#if WITH_METAL
        Renderer.render(x);
#else
        render(window->window, x);
        SDL_GL_SwapWindow(window->window);
#endif

        // Because there are no other synchronization points in this demo
        // (we are not evaluating any results from the GPU), this wait is
        // required to prevent endless submission of asynchronous kernel calls.
        f.wait();
    }
    return 0;
}

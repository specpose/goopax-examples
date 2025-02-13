/**
   \example deep-zoom-mandelbrot.cpp
   Mandelbrot example program with deep zoom capability.
 */

#include "common/draw/window_sdl.h"
#if !USE_SDL2
#include <SDL3/SDL_main.h>
#endif
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <chrono>
#include <goopax_extra/struct_types.hpp>

using namespace goopax;
using namespace std;
using namespace Eigen;
using goopax::interface::PI;
using std::chrono::duration;
using std::chrono::steady_clock;

template<class A, class B>
struct pair_firstsort : public pair<A, B>
{
    pair_firstsort(const pair<A, B>& p)
        : pair<A, B>(p)
    {
    }
    pair_firstsort()
    {
    }

    template<class C>
    pair_firstsort& operator=(const C& c)
    {
        static_cast<pair<A, B>&>(*this) = c;
        return *this;
    }

    auto operator<(const pair_firstsort& b) const
    {
        return (this->first < b.first);
    }
};

template<class A, class B>
pair_firstsort<A, B> min(const pair_firstsort<A, B>& a, const pair_firstsort<A, B>& b)
{
    return cond(a.first < b.first, a, b);
}

GOOPAX_PREPARE_STRUCT(pair_firstsort)

template<typename TO, typename FROM>
complex<TO> complex_cast(const complex<FROM>& from)
{
    return complex<TO>(static_cast<typename unrangetype<TO>::type>(unrange(from.real())),
                       static_cast<typename unrangetype<TO>::type>(unrange(from.imag())));
}

template<size_t N>
using realN = boost::multiprecision::number<boost::multiprecision::cpp_bin_float<N>>;
using REAL = realN<200>;

Tuint MAX_ITER = 256;

template<typename D, typename E, typename scale_t = REAL, typename window_size_t = unsigned int>
complex<D> calc_c(complex<D> center, scale_t scale, Vector<E, 2> position, window_size_t window_size)
{
    // Calculate the value c for a given image point.
    // This function is called both from within the kernel and from the main function
    return center
           + static_cast<D>(scale / window_size[0])
                 * complex<D>(position[0] - window_size[0] * 0.5f, position[1] - window_size[1] * 0.5f);
}

complex<REAL> moveto = {
    REAL("-1."
         "2603623238862161410225762753413208710717617498969566568620680540827846649972224086588936456568544934106441786"
         "339417760037065256521411260061136660887905828358899285625388641156809047832482164262411671"),
    REAL("-0."
         "3553257536319313812280478909820294005717444145506363484173742044027446521308091651532591805538781639164678355"
         "9812469373640793642356376154715067642695987014146996961508736162323693119329311610190435137")
};

pair<double, double> scalerange = { 2, 2E-67 };

double deltat = 92;
bool manual_mode = false;

realN<10> scale = 2.4161963835763931682e-3f;

complex<REAL> center = moveto;
float speed_zoom = 1E-2;
auto mandelbrot_lasttime = steady_clock::now();
auto mandelbrot_timebegin = steady_clock::now();

class mandelbrot
{
    complex<REAL> c0 = { 0, 0 };

public:
    buffer<complex<Tfloat>> z_centervals;
    Tuint size_centervals = 0;
    shared_ptr<sdl_window> window;
    kernel<void(image_buffer<2, Vector<Tuint8_t, 4>, true>& image,
                const Tfloat scale_m,
                const Tuint scale_exp,
                Tuint max_iter,
                const buffer<complex<Tfloat>>& z_centervals,
                const complex<Tfloat> center_offset_m,
                goopax_future<pair_firstsort<float, complex<float>>>& best_dc_m,
                goopax_future<unsigned int>& want_more)>
        Kernel;

    void set_z0()
    {
        // Compute high precision reference data for single point c0.

        if (z_centervals.size() < MAX_ITER)
        {
            z_centervals = buffer<complex<Tfloat>>(window->device, MAX_ITER);
        }

        buffer_map<complex<Tfloat>> z_centervalsMap(z_centervals, 0, MAX_ITER, BUFFER_WRITE_DISCARD);
        complex<REAL> zc = { 0, 0 };
        for (Tuint k = 0; k < MAX_ITER; ++k)
        {
            z_centervalsMap[k] = complex_cast<Tfloat>(zc);
            if (norm(zc) < 1E10)
            {
                zc = zc * zc + c0;
            }
        }
        size_centervals = MAX_ITER;
    }

    void render()
    {
        static auto frametime = steady_clock::now();
        static Tint framecount = 0;

        auto now = steady_clock::now();

        array<unsigned int, 2> fbsize = window->get_size();

        ++framecount;
        if (now - frametime > std::chrono::seconds(1))
        {
            stringstream title;
            auto rate = framecount / duration<double>(now - frametime).count();
            title << "Mandelbrot: screen size=" << fbsize[0] << "x" << fbsize[1] << ", " << rate
                  << " fps, scale=" << scale << ", max_iter=" << MAX_ITER << ", device=" << window->device.name();

            window->set_title(title.str());
            framecount = 0;
            frametime = now;
        }

        if (!manual_mode)
        {
            double wait = 1;
            auto t = (duration<double>(now - mandelbrot_timebegin).count() - wait) / (deltat - wait);
            t = max(t, 0.);
            t = min(t, 1.);
            double x = 0.5 - 0.5 * cos(t * PI);
            scale = realN<10>(exp(log(scalerange.first) * (1 - x) + log(scalerange.second) * x));
        }
        else
        {
            float dt = duration<float>(now - mandelbrot_lasttime).count();

            center += (moveto - center) * complex<REAL>(max(0.4, abs(speed_zoom) * 1.1) * dt);
            scale *= exp(speed_zoom * dt);
            speed_zoom *= exp(-dt / 10);
        }
        mandelbrot_lasttime = now;

        set_z0();

        complex<REAL> center_offset = center - c0;
        Tuint shift = 0;
        REAL scale_m = scale;
        auto center_offset_m = center_offset;
        while (scale_m < 1.0 / static_cast<float>(fbsize[0]))
        {
            ++shift;
            scale_m *= 2;
            center_offset_m *= 2;
        }

        goopax_future<unsigned int> want_more;
        {
            goopax_future<pair_firstsort<float, complex<float>>> best_dc;

            window->draw_goopax([&](image_buffer<2, Vector<Tuint8_t, 4>, true>& image) {
                this->Kernel(image,
                             static_cast<float>(scale_m),
                             shift,
                             MAX_ITER,
                             this->z_centervals,
                             complex<float>((float)center_offset_m.real(), (float)center_offset_m.imag()),
                             best_dc,
                             want_more); // Call the kernel
            });

            if (best_dc.get().first != Tfloat(1E10f))
            {
                // Shifting reference point to the best prospect.
                c0 += static_cast<complex<REAL>>(best_dc.get().second)
                      * static_cast<REAL>(pow((REAL)0.5, unrange(shift)));
            }
        }
        if (true)
        {
            if (want_more.get() > 0.01 * fbsize[0] * fbsize[1])
            {
                MAX_ITER = Tuint(MAX_ITER * 1.2) / 256 * 256 + 256;
            }
            else if (want_more.get() > 0.002 * fbsize[0] * fbsize[1])
                MAX_ITER = Tuint(MAX_ITER * 1.02) / 256 * 256 + 256;
            else if (want_more.get() < 0.001 * fbsize[0] * fbsize[1])
            {
                if (MAX_ITER > 256)
                    MAX_ITER -= 256;
            }
        }
    }

    mandelbrot(shared_ptr<sdl_window> window0)
        : window(window0)
    {
        Kernel.assign(window->device,
                      [](image_resource<2, Vector<Tuint8_t, 4>, true>& image,
                         const gpu_float scale_m,
                         const gpu_uint scale_exp,
                         gpu_uint max_iter,
                         const resource<complex<Tfloat>>& z_centervals,
                         const complex<gpu_float> center_offset_m,
                         gather<pair_firstsort<float, complex<float>>, ::op_min>& best_dc,
                         gather_add<unsigned int>& want_more) {
                          auto& zc = z_centervals;
                          best_dc.first = 1E10f;
                          best_dc.second = numeric_limits<float>::quiet_NaN();
                          want_more = 0;

                          gpu_for_global(
                              0,
                              image.width() * image.height(),
                              [&](gpu_uint k) // Parallel loop over all image points
                              {
                                  Vector<gpu_uint, 2> position = { k % image.width(), k / image.width() };
                                  complex<gpu_float> dc = calc_c<gpu_float>(
                                      center_offset_m, scale_m, position.cast<gpu_float>().eval(), image.dimensions());

                                  complex<gpu_float> dc_orig = dc;
                                  complex<gpu_float> dz(0, 0);
                                  complex<gpu_float> z(0, 0);

                                  // The aim of the shift/scalefac variables is to prevent underflow when
                                  // floating point numbers get very small.
                                  gpu_uint shift = scale_exp;

                                  gpu_float maxz = 0;
                                  gpu_uint iter = 0;

                                  // As soon as norm(z) >= 4, z is destined to diverge to inf. Delaying
                                  // until 10.f to make colors a bit smoother.
                                  gpu_while(iter < max_iter && norm(z) < 10.f)
                                  {
                                      gpu_uint s2u = reinterpret<Tuint>(1.f);
                                      s2u -= shift << 23;
                                      auto scale2 = reinterpret<gpu_float>(s2u);
                                      gpu_float scalefac = cond(shift >= 127u, 0, (gpu_float)scale2);

                                      z = zc[iter] + dz * scalefac;
                                      maxz = max(maxz, (gpu_float)norm(zc[iter] + dz * scalefac));

                                      // The core formula of the mandelbrot set, computed as deviation from
                                      // the reference point (z = z0 + dz).
                                      dz = zc[iter] * dz * gpu_float(2) + dz * dz * scalefac + dc;

                                      gpu_while(norm(dz) > 1 && shift != 0)
                                      {
                                          dz = dz * gpu_float(0.5f);
                                          dc = dc * gpu_float(0.5f);
                                          --shift;
                                          scalefac *= 2;
                                      }
                                      ++iter;
                                  }

                                  Vector<gpu_float, 4> color = { 0, 0, 0.4, 1 };

                                  gpu_if(norm(z) >= 4.f)
                                  {
                                      gpu_float x = (iter - log2(log2(norm(z)))) * 0.1f;
                                      color[0] = 0.5f + 0.5f * sin(x);
                                      color[1] = 0.5f + 0.5f * sin(x + static_cast<float>(PI * 2. / 3));
                                      color[2] = 0.5f + 0.5f * sin(x + static_cast<float>(PI * 4. / 3));
                                  }

                                  image.write(position, color); // Set the color according to the escape time

                                  want_more += gpu_uint(iter + 256 > max_iter * 0.7f && iter != max_iter);

                                  gpu_float value =
                                      cond(iter == max_iter, -(gpu_float)iter + maxz - 10, -(gpu_float)iter);
                                  gpu_if(value < best_dc.first)
                                  {
                                      best_dc.first = value;
                                      best_dc.second = dc_orig;
                                  }
                              });
                      });
    }
};

int main(int, char**)
{
    shared_ptr<sdl_window> window = sdl_window::create("deep zoom mandelbrot",
                                                       Eigen::Vector<Tuint, 2>{ 640, 480 },
                                                       SDL_WINDOW_RESIZABLE |
#if !USE_SDL2
                                                       SDL_WINDOW_HIGH_PIXEL_DENSITY
#else
                                                       SDL_WINDOW_ALLOW_HIGHDPI
#endif
                                                       );

    using namespace boost::multiprecision;

    bool quit = false;
    bool is_fullscreen = false;

    bool fingermotion_active = false;
    bool too_many_fingers = false;
    int num_fingers = 0;
    Vector<float, 2> last_fingermotion;

    mandelbrot Mandelbrot(window);

    while (!quit)
    {
        while (auto e = window->get_event())
        {
            array<unsigned int, 2> window_size = window->get_size();
#if !USE_SDL2
            if (e->type == SDL_EVENT_QUIT)
#else
            if (e->type == SDL_QUIT)
#endif
            {
                quit = true;
            }
#if !USE_SDL2
            else if (e->type == SDL_EVENT_FINGER_DOWN)
#else
            else if (e->type == SDL_FINGERDOWN)
#endif
            {
                ++num_fingers;
                cout << "num_fingers=" << num_fingers << endl;
                if (num_fingers == 2)
                {
                    too_many_fingers = true;
                }
                fingermotion_active = false;
            }
#if !USE_SDL2
            else if (e->type == SDL_EVENT_FINGER_UP)
#else
            else if (e->type == SDL_FINGERUP)
#endif
            {
                --num_fingers;
                cout << "num_fingers=" << num_fingers << endl;
                if (num_fingers == 0)
                {
                    too_many_fingers = false;
                }
            }
#if !USE_SDL2
            else if (e->type == SDL_EVENT_FINGER_MOTION)
#else
            else if (e->type == SDL_FINGERMOTION)
#endif
            {
                cout << "fingermotion. x=" << e->tfinger.x << ", y=" << e->tfinger.y << endl;

                if (fingermotion_active && !too_many_fingers)
                {
                    const complex<REAL> shift =
                        calc_c(center,
                               scale,
                               Vector<double, 2>{ e->tfinger.x * window_size[0], e->tfinger.y * window_size[1] },
                               window_size)
                        - calc_c(center,
                                 scale,
                                 Vector<double, 2>{ last_fingermotion[0] * window_size[0],
                                                    last_fingermotion[1] * window_size[1] },
                                 window_size);
                    cout << "shift=" << shift << endl;

                    center -= shift;
                    moveto -= shift;
                }
                last_fingermotion = { e->tfinger.x, e->tfinger.y };
                fingermotion_active = true;
            }
#if !USE_SDL2
            else if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
#else
            else if (e->type == SDL_MOUSEBUTTONDOWN)
#endif
            {
#if !USE_SDL2
                float x = 0, y = 0;
#else
                int x = 0, y = 0;
#endif
                SDL_GetMouseState(&x, &y);
                cout << "Mouse button " << e->button.button << ". x=" << x << ", y=" << y << endl;

                if (true)
                {

                    moveto = calc_c<REAL>(center,
                                          scale, // Set new center
                                          Vector<double, 2>{ x, y },
                                          window_size);
                    cout.precision(max(int(-log10(scale)) + 5, 5));
                    cout << "new center=" << moveto;
                    cout.precision(10);
                    cout << ", scale=" << scale << endl;
                    manual_mode = true;
                }
            }
#if !USE_SDL2
            else if (e->type == SDL_EVENT_MOUSE_WHEEL)
#else
            else if (e->type == SDL_MOUSEWHEEL)
#endif
            {
                speed_zoom -= e->wheel.y;
                manual_mode = true;
            }
            /*
              else if (e->type == SDL_MULTIGESTURE)
                {
                    if (fabs(e->mgesture.dDist) > 0.002)
                    {
                        speed_zoom -= 10 * e->mgesture.dDist;
                    }
                }
            */
#if !USE_SDL2
            else if (e->type == SDL_EVENT_KEY_DOWN)
            {

                switch (e->key.key)
                {
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                    case SDLK_F: {
#else
            else if (e->type == SDL_KEYDOWN)
            {

                switch (e->key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                    case SDLK_f: {
#endif
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
        Mandelbrot.render();
    }
    return 0;
}

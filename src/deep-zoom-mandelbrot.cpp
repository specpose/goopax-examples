#include "common/draw/window_sdl.h"
#include <boost/multiprecision/gmp.hpp>
#include <chrono>
#include <goopax_extra/struct_types.hpp>

using namespace goopax;
using namespace std;
using namespace Eigen;
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
using realN = boost::multiprecision::number<boost::multiprecision::gmp_float<N>>;
using REAL = realN<200>;

Tuint MAX_ITER = 256;

template<typename D, typename E>
complex<D> calc_c(complex<D> center, auto scale, Vector<E, 2> position, auto window_size)
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

pair<double, double> scalerange = { 5e-3, 2E-70 };

double deltat = 92;
bool manual_mode = false;

realN<10> scale = 2.4161963835763931682e-3;

complex<REAL> force_c0 = moveto;
Tuint force_c0_count = 1000000000;

complex<REAL> center = moveto;
double speed_zoom = 1E-2;
auto mandelbrot_lasttime = steady_clock::now();
auto mandelbrot_timebegin = steady_clock::now();

static complex<REAL> c0_try = { 0, 0 };
static complex<REAL> c0 = { 0, 0 };

class mandelbrot
{
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
                goopax_future<uint>& want_more)>
        Kernel; // Kernel code goes into this function

    void set_z0()
    {
        if (z_centervals.size() < MAX_ITER)
        {
            z_centervals = buffer<complex<Tfloat>>(window->device, MAX_ITER);
        }
        buffer_map<complex<Tfloat>> z_centervalsMap(z_centervals, BUFFER_WRITE | BUFFER_DISCARD);

        complex<REAL> oldc = c0;
        c0 += c0_try;

        if (force_c0_count != 0)
        {
            c0 = force_c0;
        }

        if (c0 != oldc || MAX_ITER > size_centervals)
        {
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
        if (force_c0_count)
            --force_c0_count;
    }

    static Vector<gpu_float, 4> color(const gpu_uint e,
                                      gpu_uint max_iter) // The color palette is outsourced into this function
    {
        const Vector<gpu_float, 4> c1 = { 0xb0, 0xbc, 0x3d, 0xff }; // desired RGB colors
        const Vector<gpu_float, 4> c2 = { 0x7e, 0x92, 0x6d, 0xff };
        const Vector<gpu_float, 4> c3 = { 0x44, 0x62, 0x61, 0xff };
        Vector<gpu_float, 4> ret;

        {
            const Vector<gpu_float, 4> d1 = (c1 - c2 / 64 - c3 / 64) / 255; // Adjusting colors
            const Vector<gpu_float, 4> d2 = (c2 - c1 / 64 - c3 / 64) / 255;
            const Vector<gpu_float, 4> d3 = (c3 - c1 / 64 - c2 / 64) / 255;

            gpu_float f = e / 256.0f;

            ret = d1 * pow<4, 1>(sinpi(f)) + d2 * pow<4, 1>(sinpi(f - 1.0f / 3)) + d3 * pow<6, 1>(sinpi(f - 2.0f / 3));

            ret *= cond(e == max_iter - 1, 0, min(f * 2 + 0.2f, 1));
        }
        ret[3] = 1;
        return ret;
    }

    void render()
    {
        static auto frametime = steady_clock::now();
        static Tint framecount = 0;

        auto now = steady_clock::now();

        Vector<Tuint, 2> fbsize = window->get_size();

        ++framecount;
        if (now - frametime > std::chrono::seconds(1))
        {
            stringstream title;
            auto rate = framecount / duration<double>(now - frametime).count();
            title << "Mandelbrot: screen size=" << fbsize[0] << "x" << fbsize[1] << ", " << rate
                  << " fps, scale=" << scale << ", max_iter=" << MAX_ITER;

            string s = title.str();
            SDL_SetWindowTitle(window->window, s.c_str());
            framecount = 0;
            frametime = now;
        }

        if (!manual_mode)
        {
            double wait = 1;
            auto t = (duration<double>(now - mandelbrot_timebegin).count() - wait) / (deltat - wait);
            t = max(t, 0.);
            t = min(t, 1.);
            double x = 0.5 - 0.5 * cos(t * M_PI);
            scale = realN<10>(exp(log(scalerange.first) * (1 - x) + log(scalerange.second) * x));
        }
        else
        {
            double dt = duration<double>(now - mandelbrot_lasttime).count();

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

        goopax_future<uint> want_more;
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
                c0_try = static_cast<complex<REAL>>(best_dc.get().second)
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
                         gather<pair_firstsort<Tfloat, complex<Tfloat>>, ::op_min>& best_dc_m,
                         gather_add<Tuint>& want_more) // Kernel code goes into this function
                      {
                          auto& zc = z_centervals;
                          best_dc_m.first = 1E10f;
                          want_more = 0;

                          gpu_for_global(
                              0,
                              image.width() * image.height(),
                              num_subthreads(),
                              [&](gpu_uint k) // Parallel loop over all image points
                              {
                                  vector<complex<gpu_float>> dc_m(num_subthreads());
                                  for (Tuint t = 0; t < num_subthreads(); ++t)
                                  {
                                      dc_m[t] = calc_c<gpu_float>(
                                          center_offset_m,
                                          scale_m,
                                          Vector<gpu_float, 2>{ (k + t) % image.width(), (k + t) / image.width() },
                                          image.dimensions());
                                  }
                                  auto dc_m_orig = dc_m;
                                  vector<complex<gpu_float>> dz_m(num_subthreads(), complex<gpu_float>(0, 0));
                                  vector<gpu_uint> e(num_subthreads(), 0);
                                  vector<gpu_uint> shift(num_subthreads(), scale_exp);

                                  gpu_float maxz = 0;
                                  gpu_for(0, max_iter, 16, [&](gpu_uint ibase) {
                                      vector<gpu_float> scalefac(num_subthreads());
                                      for (Tuint t = 0; t < num_subthreads(); ++t)
                                      {
                                          gpu_uint s2u = reinterpret<Tuint>(1.f);
                                          s2u -= shift[t] << 23;
                                          auto scale2 = reinterpret<gpu_float>(s2u);

                                          scalefac[t] = cond(shift[t] >= 127u, 0, (gpu_float)scale2);
                                      }
                                      gpu_for(ibase, ibase + 16, [&](gpu_uint i) {
                                          for (Tuint t = 0; t < num_subthreads(); ++t)
                                          {
                                              e[t] = cond(norm(zc[i] + dz_m[t] * scalefac[t]) < 4.f, i, e[t]);
                                              if (t == 0)
                                                  maxz = max(maxz, (gpu_float)norm(zc[i] + dz_m[t] * scalefac[t]));
                                              dz_m[t] = zc[i] * dz_m[t] * gpu_float(2) + dz_m[t] * dz_m[t] * scalefac[t]
                                                        + dc_m[t]; // The core formula of the mandelbrot set
                                              gpu_while(norm(dz_m[t]) > 1 && shift[t] != 0)
                                              {
                                                  dz_m[t] = dz_m[t] * gpu_float(0.5f);
                                                  dc_m[t] = dc_m[t] * gpu_float(0.5f);
                                                  --shift[t];
                                                  scalefac[t] *= 2;
                                              }
                                          }
                                      });
                                      auto maxe = e[0];
                                      for (Tuint k = 1; k < e.size(); ++k)
                                      {
                                          maxe = max(maxe, e[k]);
                                      }
                                      gpu_if(maxe != ibase + 15) ibase.gpu_break();
                                  });
                                  for (Tuint t = 0; t < num_subthreads(); ++t)
                                  {
                                      gpu_uint y = (k + t) / image.width();
                                      gpu_uint x = (k + t) % image.width();

                                      Vector<gpu_float, 4> c = color(e[t], max_iter);

                                      image.write({ x, y }, c); // Set the color according to the escape time
                                      want_more += gpu_uint(e[t] + 256 > max_iter * 0.7f && e[t] != max_iter - 1);
                                  }
                                  gpu_float value =
                                      cond(e[0] == max_iter - 1, -(gpu_float)e[0] + maxz - 10, -(gpu_float)e[0]);
                                  gpu_if(value < best_dc_m.first)
                                  {
                                      best_dc_m.first = value;
                                      best_dc_m.second = dc_m_orig[0];
                                  }
                              });
                      });
    }
};

int main(int argc, char** argv)
{
    shared_ptr<sdl_window> window = sdl_window::create(
        "deep zoom mandelbrot", Eigen::Vector<Tuint, 2>{ 640, 480 }, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

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
            Vector<unsigned int, 2> window_size = window->get_size().cast<unsigned int>();
            if (e->type == SDL_QUIT)
            {
                quit = true;
            }
            else if (e->type == SDL_FINGERDOWN)
            {
                ++num_fingers;
                cout << "num_fingers=" << num_fingers << endl;
                if (num_fingers == 2)
                {
                    too_many_fingers = true;
                }
                fingermotion_active = false;
            }
            else if (e->type == SDL_FINGERUP)
            {
                --num_fingers;
                cout << "num_fingers=" << num_fingers << endl;
                if (num_fingers == 0)
                {
                    too_many_fingers = false;
                }
            }

            else if (e->type == SDL_FINGERMOTION)
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

            else if (e->type == SDL_MOUSEBUTTONDOWN)
            {
                int x = 0, y = 0;
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
                    force_c0_count = 0;
                    manual_mode = true;
                }
            }
            else if (e->type == SDL_MOUSEWHEEL)
            {
                speed_zoom -= e->wheel.y;
                manual_mode = true;
            }
            else if (e->type == SDL_MULTIGESTURE)
            {
                if (fabs(e->mgesture.dDist) > 0.002)
                {
                    speed_zoom -= 10 * e->mgesture.dDist;
                }
            }

            else if (e->type == SDL_KEYDOWN)
            {
                cout << "keydown. sym=" << e->key.keysym.sym << ", name=" << SDL_GetKeyName(e->key.keysym.sym) << endl;
                switch (e->key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                    case SDLK_f: {
                        int err = SDL_SetWindowFullscreen(window->window,
                                                          (is_fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP));
                        if (err == 0)
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

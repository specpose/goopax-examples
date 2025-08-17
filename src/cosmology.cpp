/**
   \example cosmology.cpp
   FMM N-body example program

   It is a rather complex algorithm, roughly based on
   "A short course on fast multiple methods", https://math.nyu.edu/~greengar/shortcourse_fmm.pdf,
   but with some modifications:
   - Multipoles are represented in Cartesian coordinates instead of the usual spherical harmonics.
   - A binary tree is used instead of an octree.

   The parameters are optimise for big GPUs with many registers.
   If you want to run it on smaller GPUs with <256 registers,
   you might want to reduce MULTIPOLE_ORDER to 2 or so. The precision will be worse,
   but at least it will run with usable performance.
 */

#define WITH_TIMINGS 0

#if __has_include(<opencv2/opencv.hpp>)
#include <opencv2/opencv.hpp>
#define WITH_OPENCV 1
#else
#define WITH_OPENCV 0
#endif

#include "common/particle.hpp"
#include <SDL3/SDL_main.h>
#if WITH_TIMINGS
#include <chrono>
#endif
#include <fstream>
#include <goopax_draw/window_sdl.h>
#include <goopax_extra/output.hpp>
#include <goopax_extra/param.hpp>
#include <random>
#include <set>

using Eigen::Vector;
using Eigen::Vector3;
using Eigen::Vector4;
using namespace goopax;
using namespace std;
using chrono::duration;
using chrono::steady_clock;
using chrono::time_point;

constexpr size_t intceil(const size_t a, size_t mod)
{
    return a + (mod - ((mod + a - 1) % mod) - 1);
}

template<typename S>
concept ostream_type = std::same_as<S, std::ostream> || std::same_as<S, goopax::gpu_ostream>;

template<ostream_type S, typename V>
#if __cpp_lib_ranges >= 201911
    requires std::ranges::input_range<V>
#else
    requires std::is_convertible<typename std::iterator_traits<typename V::iterator>::iterator_category,
                                 std::input_iterator_tag>::value
#endif
             && std::is_class<V>::value && (!is_same<V, string>::value)
             && (is_same<S, ostream>::value || is_same<S, goopax::gpu_ostream>::value)
S& operator<<(S& s, const V& v)
{
    s << "(";
    for (auto t = v.begin(); t != v.end(); ++t)
    {
        if (t != v.begin())
        {
            s << ", ";
        }
        s << *t;
    }
    s << ")";
    return s;
}

namespace std
{
template<typename A, typename B>
ostream& operator<<(ostream& s, const pair<A, B>& p)
{
    return s << "[" << p.first << "," << p.second << "]";
}
}

using signature_t = Tuint64_t;
using gpu_signature_t = typename make_gpu<signature_t>::type;

using CTuint = Tuint;
using CTfloat = Tfloat;

#define cout1  \
    if (false) \
    cout
#define DEBUG1 false
#define VERB1 false

Tuint log2_exact(Tsize_t a)
{
    for (unsigned int r = 0; r < 61; ++r)
    {
        if (a == (1ul << r))
        {
            return r;
        }
    }
    cout << "log2_exact: bad a=" << a << endl;
    abort();
}

#include "radix_sort.hpp"
const float halflen = 4;
PARAMOPT<Tfloat> MULTIPOLE_COSTFAC("multipole_costfac", 160);
PARAMOPT<Tuint> MAX_BIGNODE_BITS("max_bignode_bits", 3);
PARAMOPT<Tuint> MAX_NODESIZE("max_nodesize", 16);
PARAMOPT<Tuint> MAX_DEPTH("max_depth", 64);
PARAMOPT<Tbool> POW2_SIZEVEC("pow2_sizevec", true);
#define CALC_POTENTIAL 1

PARAMOPT<string> IC("ic", "");

PARAMOPT<Tsize_t> NUM_PARTICLES("num_particles", 1000000); // Number of particles
PARAMOPT<Tdouble> DT("dt", 5E-3);
PARAMOPT<Tdouble> MAX_DISTFAC("max_distfac", 1.2);

PARAMOPT<bool> PRECISION_TEST("precision_test", false);

constexpr unsigned int MULTIPOLE_ORDER = 4;

using GPU_DOUBLE = gpu_double;
using TDOUBLE = Tdouble;
using CTDOUBLE = Tdouble;

template<class T>
Vector<T, 4> color(T pot)
{
    T pc = log2(clamp((-pot - 0.0f) * 0.6f, 1, 15.99f));

    gpu_float slot = floor(pc);
    gpu_float x = pc - slot;
    gpu_assert(slot >= 0);
    gpu_assert(slot < 4);
    Vector<T, 4> ret =
        cond(slot == 0,
             Vector<gpu_float, 4>({ 0, x, 1 - x, 0 }),
             cond(slot == 1,
                  Vector<gpu_float, 4>({ x, 1 - x, 0, 0 }),
                  cond(slot == 2, Vector<gpu_float, 4>({ 1, x, 0, 0 }), Vector<gpu_float, 4>({ 1, 1, x, 0 }))));
    return ret;
}

template<unsigned int N>
vector<vector<Tuint>> make_indices()
{
    vector<vector<Tuint>> sub = make_indices<N - 1>();
    vector<vector<Tuint>> ret;
    for (auto s : sub)
    {
        for (Tuint k = 0; k < 3; ++k)
        {
            ret.push_back(s);
            ret.back().push_back(k);
        }
    }
    return ret;
}
template<>
vector<vector<Tuint>> make_indices<0>()
{
    return { {} };
}

template<unsigned int N>
vector<Tuint> make_mi()
{
    std::map<vector<Tuint>, Tuint> have;
    vector<Tuint> ret;
    Tuint pos = 0;
    auto indices = make_indices<N>();
    for (auto i : indices)
    {
        vector<Tuint> s = i;
        sort(s.begin(), s.end());
        if (have.find(s) == have.end())
            have[s] = pos++;
        ret.push_back(have[s]);
    }
    return ret;
}

const auto MI2 = reinterpret<Vector<Vector<Tuint, 3>, 3>>(make_mi<2>());
const auto MI3 = reinterpret<Vector<Vector<Vector<Tuint, 3>, 3>, 3>>(make_mi<3>());
const auto MI4 = reinterpret<Vector<Vector<Vector<Vector<Tuint, 3>, 3>, 3>, 3>>(make_mi<4>());

template<class T>
Vector<T, 3> rot(const Vector<T, 3>& a, Tint step = 1)
{
    if (step < 0)
        return rot(a, step + 3);
    if (step == 0)
        return a;
    return rot(Vector<T, 3>({ a[1], a[2], a[0] }), step - 1);
}

template<class T, unsigned int N>
struct multipole
{
    using goopax_struct_type = T;
    template<typename X>
    using goopax_struct_changetype = multipole<typename goopax_struct_changetype<T, X>::type, N>;
    using uint_type = typename change_gpu_mode<unsigned int, T>::type;

    static constexpr size_t datasize = 1 + (N >= 1) * 3 + (N >= 2) * 6 + (N >= 3) * 10 + (N >= 4) * 15;

    T A;
    Vector<T, 3 * (N >= 1)> B;
    Vector<T, 6 * (N >= 2)> C;
    Vector<T, 10 * (N >= 3)> D;
    Vector<T, 15 * (N >= 4)> E;

    multipole rot(Tint step = 1) const
    {
        if (step < 0)
            return rot(step + 3);
        if (step == 0)
            return *this;
        multipole ret;
        ret.A = A;
        for (Tuint i = 0; i < 3; ++i)
        {
            Tuint io = (i + 1) % 3;
            if (N >= 1)
                ret.B[i] = B[io];
            for (Tuint k = 0; k < 3; ++k)
            {
                Tuint ko = (k + 1) % 3;
                if (N >= 2)
                    ret.C[MI2[i][k]] = C[MI2[io][ko]];
                for (Tuint l = 0; l < 3; ++l)
                {
                    Tuint lo = (l + 1) % 3;
                    if (N >= 3)
                        ret.D[MI3[i][k][l]] = D[MI3[io][ko][lo]];
                    for (Tuint m = 0; m < 3; ++m)
                    {
                        Tuint mo = (m + 1) % 3;
                        if (N >= 4)
                            ret.E[MI4[i][k][l][m]] = E[MI4[io][ko][lo][mo]];
                    }
                }
            }
        }
        return ret.rot(step - 1);
    }

    template<class U>
    multipole(const multipole<U, N>& b)
        : A(b.A)
    {
        std::copy(b.B.begin(), b.B.end(), B.begin());
        std::copy(b.C.begin(), b.C.end(), C.begin());
        std::copy(b.D.begin(), b.D.end(), D.begin());
        std::copy(b.E.begin(), b.E.end(), E.begin());
    }

    multipole()
    {
    }

    multipole& operator+=(const multipole& b)
    {
        A += b.A;
        if (N >= 1)
            B += b.B;
        if (N >= 2)
            C += b.C;
        if (N >= 3)
            D += b.D;
        if (N >= 4)
            E += b.E;
        return *this;
    }

    static multipole from_particle(Vector<T, 3> a, T mass, uint_type ID = 0)
    {
        (void)ID;
        a = -a;
        multipole M;
        if (N >= 0)
        {
            M.A = -mass;
        }
        if (N >= 1)
        {
            for (Tuint k = 0; k < 3; ++k)
            {
                M.B[k] = (-mass) * a[k];
            }
        }
        if (N >= 2)
        {
            for (Tuint i = 0; i < 3; ++i)
                for (Tuint k = i; k < 3; ++k)
                {
                    M.C[MI2[i][k]] = (-mass) * (1.5f * a[i] * a[k] - 0.5f * int(i == k) * a.squaredNorm());
                }
        }
        if (N >= 3)
        {
            for (Tuint i = 0; i < 3; ++i)
                for (Tuint k = i; k < 3; ++k)
                    for (Tuint l = k; l < 3; ++l)
                    {
                        M.D[MI3[i][k][l]] =
                            (-mass)
                            * (2.5f * a[i] * a[k] * a[l]
                               - 0.5f * a.squaredNorm()
                                     * (a[i] * Tint(k == l) + a[k] * Tint(i == l) + a[l] * Tint(i == k)));
                    }
        }
        if (N >= 4)
        {
            for (Tuint i = 0; i < 3; ++i)
                for (Tuint k = i; k < 3; ++k)
                    for (Tuint l = k; l < 3; ++l)
                        for (Tuint m = l; m < 3; ++m)
                        {
                            M.E[MI4[i][k][l][m]] =
                                (-mass)
                                * (35.0f / 8 * a[i] * a[k] * a[l] * a[m]
                                   - 5.0f / 8
                                         * (a[i] * a[k] * Tint(l == m) + a[i] * a[l] * Tint(k == m)
                                            + a[i] * a[m] * Tint(k == l) + a[k] * a[l] * Tint(i == m)
                                            + a[k] * a[m] * Tint(i == l) + a[l] * a[m] * Tint(i == k))
                                         * a.squaredNorm()
                                   + 1.0f / 8 * pow2(a.squaredNorm())
                                         * (Tint(i == k) * Tint(l == m) + Tint(i == l) * Tint(k == m)
                                            + Tint(i == m) * Tint(k == l)));
                        }
        }
        return M;
    }

    multipole shift_ext(Vector<T, 3> a) const
    {
        multipole M = *this;

        if (N >= 1)
        {
            M.B += a * A;
        }
        if (N >= 2)
        {
            for (Tuint i = 0; i < 3; ++i)
                for (Tuint k = i; k < 3; ++k)
                {
                    M.C[MI2[i][k]] += 1.5f * a[i] * a[k] * A - 0.5f * A * a.squaredNorm() * Tint(i == k)
                                      + 1.5f * (B[i] * a[k] + B[k] * a[i]);
                    for (Tuint n = 0; n < 3; ++n)
                    {
                        M.C[MI2[i][k]] += -B[n] * a[n] * Tint(i == k);
                    }
                }
        }
        if (N >= 3)
        {
            for (Tuint i = 0; i < 3; ++i)
                for (Tuint k = i; k < 3; ++k)
                    for (Tuint l = k; l < 3; ++l)
                    {
                        M.D[MI3[i][k][l]] += 2.5f * A * a[i] * a[k] * a[l]
                                             - 0.5f * A * a.squaredNorm()
                                                   * (a[i] * Tint(k == l) + a[k] * Tint(i == l) + a[l] * Tint(i == k))
                                             + static_cast<T>(5.0 / 3)
                                                   * (C[MI2[i][k]] * a[l] + C[MI2[i][l]] * a[k] + C[MI2[k][l]] * a[i])
                                             + 5.0f / 2 * (B[i] * a[k] * a[l] + B[k] * a[i] * a[l] + B[l] * a[i] * a[k])
                                             - 1.0f / 2 * a.squaredNorm()
                                                   * (B[i] * Tint(k == l) + B[k] * Tint(i == l) + B[l] * Tint(i == k));
                        for (Tuint n = 0; n < 3; ++n)
                        {
                            M.D[MI3[i][k][l]] +=
                                -static_cast<T>(2.0 / 3) * a[n]
                                    * (C[MI2[n][k]] * Tint(i == l) + C[MI2[n][i]] * Tint(k == l)
                                       + C[MI2[n][l]] * Tint(i == k))
                                - a[n] * B[n] * (a[i] * Tint(k == l) + a[k] * Tint(i == l) + a[l] * Tint(i == k));
                        }
                    }
        }
        if (N >= 4)
        {
            for (Tuint i = 0; i < 3; ++i)
                for (Tuint k = i; k < 3; ++k)
                    for (Tuint l = k; l < 3; ++l)
                        for (Tuint m = l; m < 3; ++m)
                        {
                            M.E[MI4[i][k][l][m]] +=
                                35.0f / 8 * A * a[i] * a[k] * a[l] * a[m]
                                - 5.0f / 8 * A * a.squaredNorm()
                                      * (a[i] * a[k] * Tint(l == m) + a[i] * a[l] * Tint(k == m)
                                         + a[i] * a[m] * Tint(k == l) + a[k] * a[l] * Tint(i == m)
                                         + a[k] * a[m] * Tint(i == l) + a[l] * a[m] * Tint(i == k))
                                + 1.0f / 8 * A * pow2(a.squaredNorm())
                                      * (Tint(i == k) * Tint(l == m) + Tint(i == l) * Tint(k == m)
                                         + Tint(i == m) * Tint(k == l))
                                + 7.0f / 4
                                      * (D[MI3[i][k][l]] * a[m] + D[MI3[i][k][m]] * a[l] + D[MI3[i][l][m]] * a[k]
                                         + D[MI3[k][l][m]] * a[i])
                                + static_cast<T>(35.0 / 12)
                                      * (C[MI2[i][k]] * a[l] * a[m] + C[MI2[i][l]] * a[k] * a[m]
                                         + C[MI2[i][m]] * a[k] * a[l] + C[MI2[k][l]] * a[i] * a[m]
                                         + C[MI2[k][m]] * a[i] * a[l] + C[MI2[l][m]] * a[i] * a[k])
                                - static_cast<T>(5.0 / 12) * a.squaredNorm()
                                      * (C[MI2[i][k]] * Tint(l == m) + C[MI2[i][l]] * Tint(k == m)
                                         + C[MI2[i][m]] * Tint(k == l) + C[MI2[k][l]] * Tint(i == m)
                                         + C[MI2[k][m]] * Tint(i == l) + C[MI2[l][m]] * Tint(i == k))
                                + static_cast<T>(35.0 / 8)
                                      * (B[i] * a[k] * a[l] * a[m] + B[k] * a[i] * a[l] * a[m]
                                         + B[l] * a[i] * a[k] * a[m] + B[m] * a[i] * a[k] * a[l])
                                - 5.0f / 8 * a.squaredNorm()
                                      * (B[i] * (a[k] * Tint(l == m) + a[l] * Tint(k == m) + a[m] * Tint(k == l))
                                         + B[k] * (a[i] * Tint(l == m) + a[l] * Tint(i == m) + a[m] * Tint(i == l))
                                         + B[l] * (a[i] * Tint(k == m) + a[k] * Tint(i == m) + a[m] * Tint(i == k))
                                         + B[m] * (a[i] * Tint(k == l) + a[k] * Tint(i == l) + a[l] * Tint(i == k)));
                            for (Tuint n = 0; n < 3; ++n)
                            {
                                M.E[MI4[i][k][l][m]] +=
                                    -0.5f * a[n]
                                        * (D[MI3[n][i][k]] * Tint(l == m) + D[MI3[n][i][l]] * Tint(k == m)
                                           + D[MI3[n][i][m]] * Tint(k == l) + D[MI3[n][k][l]] * Tint(i == m)
                                           + D[MI3[n][k][m]] * Tint(i == l) + D[MI3[n][l][m]] * Tint(i == k))
                                    - static_cast<T>(5.0 / 6) * a[n]
                                          * (C[MI2[n][i]]
                                                 * (a[k] * Tint(l == m) + a[l] * Tint(k == m) + a[m] * Tint(k == l))
                                             + C[MI2[n][k]]
                                                   * (a[i] * Tint(l == m) + a[l] * Tint(i == m) + a[m] * Tint(i == l))
                                             + C[MI2[n][l]]
                                                   * (a[i] * Tint(k == m) + a[k] * Tint(i == m) + a[m] * Tint(i == k))
                                             + C[MI2[n][m]]
                                                   * (a[i] * Tint(k == l) + a[k] * Tint(i == l) + a[l] * Tint(i == k)))
                                    - 5.0f / 4 * B[n] * a[n]
                                          * (a[i] * a[k] * Tint(l == m) + a[i] * a[l] * Tint(k == m)
                                             + a[i] * a[m] * Tint(k == l) + a[k] * a[l] * Tint(i == m)
                                             + a[k] * a[m] * Tint(i == l) + a[l] * a[m] * Tint(i == k))
                                    + 0.5f * a.squaredNorm() * a[n] * B[n]
                                          * (Tint(i == k) * Tint(l == m) + Tint(i == l) * Tint(k == m)
                                             + Tint(i == m) * Tint(k == l));
                                for (Tuint o = 0; o < 3; ++o)
                                {
                                    M.E[MI4[i][k][l][m]] += static_cast<T>(1.0 / 3) * C[MI2[n][o]] * a[n] * a[o]
                                                            * (Tint(i == k) * Tint(l == m) + Tint(i == l) * Tint(k == m)
                                                               + Tint(i == m) * Tint(k == l));
                                }
                            }
                        }
        }
        return M;
    }

    multipole shift_loc(Vector<T, 3> a) const
    {
        a = -a;
        multipole M = *this;
        if (N >= 1)
        {
            for (Tuint i = 0; i < 3; ++i)
            {
                M.A += B[i] * a[i];
            }
        }
        if (N >= 2)
        {
            for (Tuint i = 0; i < 3; ++i)
                for (Tuint k = 0; k < 3; ++k)
                {
                    M.A += C[MI2[i][k]] * a[i] * a[k];
                    M.B[i] += 2 * C[MI2[i][k]] * a[k];
                }
        }
        if (N >= 3)
        {
            for (Tuint i = 0; i < 3; ++i)
                for (Tuint k = 0; k < 3; ++k)
                    for (Tuint l = 0; l < 3; ++l)
                    {
                        M.A += D[MI3[i][k][l]] * a[i] * a[k] * a[l];
                        M.B[i] += 3 * D[MI3[i][k][l]] * a[k] * a[l];
                        if (i <= k)
                            M.C[MI2[i][k]] += 3 * D[MI3[i][k][l]] * a[l];
                    }
        }
        if (N >= 4)
        {
            for (Tuint i = 0; i < 3; ++i)
                for (Tuint k = 0; k < 3; ++k)
                    for (Tuint l = 0; l < 3; ++l)
                        for (Tuint m = 0; m < 3; ++m)
                        {
                            M.A += E[MI4[i][k][l][m]] * a[i] * a[k] * a[l] * a[m];
                            M.B[i] += 4 * E[MI4[i][k][l][m]] * a[k] * a[l] * a[m];
                            if (i <= k)
                                M.C[MI2[i][k]] += 6 * E[MI4[i][k][l][m]] * a[l] * a[m];
                            if (i <= k && k <= l)
                                M.D[MI3[i][k][l]] += 4 * E[MI4[i][k][l][m]] * a[m];
                        }
        }
        return M;
    }

    multipole makelocal(Vector<T, 3> a) const
    {
        a = -a;
        multipole M;
        T inva = pow<-1, 2>(a.squaredNorm());
        Vector<T, 3> e = a * inva;
        M.A = inva * A;
        if (N >= 1)
        {
            for (Tuint n = 0; n < 3; ++n)
            {
                M.B[n] = -pow2(inva) * A * e[n];
                M.A += pow2(inva) * B[n] * e[n];
            }
        }
        if (N >= 2)
        {
            for (Tuint i = 0; i < 3; ++i)
            {
                for (Tuint k = 0; k < 3; ++k)
                {
                    if (i <= k)
                        M.C[MI2[i][k]] = pow3(inva) * (1.5f * A * e[i] * e[k] - 0.5f * A * Tint(i == k));
                    M.B[i] += pow3(inva) * (-3 * B[k] * e[k] * e[i]);
                    M.A += pow3(inva) * C[MI2[i][k]] * e[i] * e[k];
                }
            }
            M.B += pow3(inva) * B;
        }
        if (N >= 3)
        {
            for (Tuint i = 0; i < 3; ++i)
            {
                for (Tuint k = 0; k < 3; ++k)
                {
                    for (Tuint l = 0; l < 3; ++l)
                    {
                        if (i <= k && k <= l)
                            M.D[MI3[i][k][l]] =
                                pow4(inva)
                                * (-5.0f / 2 * A * e[i] * e[k] * e[l]
                                   + 0.5f * A * (e[i] * Tint(k == l) + e[k] * Tint(i == l) + e[l] * Tint(i == k)));
                        if (i <= k)
                            M.C[MI2[i][k]] +=
                                pow4(inva)
                                * (15.0f / 2 * B[l] * e[l] * e[i] * e[k] - 1.5f * B[l] * e[l] * Tint(i == k));
                        M.B[i] += pow4(inva) * (-5 * C[MI2[k][l]] * e[k] * e[l] * e[i]);
                        M.A += pow4(inva) * (D[MI3[i][k][l]] * e[i] * e[k] * e[l]);
                    }
                    if (i <= k)
                        M.C[MI2[i][k]] += pow4(inva) * (-1.5f * (B[i] * e[k] + B[k] * e[i]));
                    M.B[i] += pow4(inva) * 2 * C[MI2[i][k]] * e[k];
                }
            }
        }
        if (N >= 4)
        {
            for (Tuint i = 0; i < 3; ++i)
            {
                for (Tuint k = 0; k < 3; ++k)
                {
                    for (Tuint l = 0; l < 3; ++l)
                    {
                        for (Tuint m = 0; m < 3; ++m)
                        {
                            if (i <= k && k <= l && l <= m)
                                M.E[MI4[i][k][l][m]] =
                                    pow5(inva)
                                    * (35.0f / 8 * A * e[i] * e[k] * e[l] * e[m]
                                       + 1.0f / 8 * A
                                             * (Tint(i == k) * Tint(l == m) + Tint(i == l) * Tint(k == m)
                                                + Tint(i == m) * Tint(k == l))
                                       - 5.0f / 8 * A
                                             * (e[i] * e[k] * Tint(l == m) + e[i] * e[l] * Tint(k == m)
                                                + e[i] * e[m] * Tint(k == l) + e[k] * e[l] * Tint(i == m)
                                                + e[k] * e[m] * Tint(i == l) + e[l] * e[m] * Tint(i == k)));
                            if (i <= k && k <= l)
                                M.D[MI3[i][k][l]] +=
                                    pow5(inva)
                                    * (-35.0f / 2 * B[m] * e[m] * e[i] * e[k] * e[l]
                                       + 5.0f / 2 * B[m] * e[m]
                                             * (e[i] * Tint(k == l) + e[k] * Tint(i == l) + e[l] * Tint(i == k)));
                            if (i <= k)
                                M.C[MI2[i][k]] += pow5(inva)
                                                  * (35.0f / 2 * C[MI2[l][m]] * e[l] * e[m] * e[i] * e[k]
                                                     - 5.0f / 2 * C[MI2[l][m]] * e[l] * e[m] * Tint(i == k));
                            M.B[i] += pow5(inva) * (-7 * D[MI3[k][l][m]] * e[k] * e[l] * e[m] * e[i]);
                            M.A += pow5(inva) * (E[MI4[i][k][l][m]] * e[i] * e[k] * e[l] * e[m]);
                        }
                        if (i <= k && k <= l)
                            M.D[MI3[i][k][l]] +=
                                pow5(inva)
                                * (+5.0f / 2 * (B[i] * e[k] * e[l] + B[k] * e[i] * e[l] + B[l] * e[i] * e[k])
                                   - 0.5f * (B[i] * Tint(k == l) + B[k] * Tint(i == l) + B[l] * Tint(i == k)));
                        if (i <= k)
                            M.C[MI2[i][k]] += pow5(inva) * (-5 * e[l] * (C[MI2[l][k]] * e[i] + C[MI2[l][i]] * e[k]));
                        M.B[i] += pow5(inva) * 3 * D[MI3[i][k][l]] * e[k] * e[l];
                    }
                    if (i <= k)
                        M.C[MI2[i][k]] += pow5(inva) * C[MI2[i][k]];
                }
            }
        }
        return M;
    }

#if CALC_POTENTIAL
    T calc_loc_potential(Vector<T, 3> r) const
    {
        r = -r;
        T ret = A;
        for (Tuint i = 0; i < 3; ++i)
        {
            if (N >= 1)
                ret += B[i] * r[i];
            for (Tuint k = i; k < 3; ++k)
            {
                if (N >= 2)
                    ret += C[MI2[i][k]] * r[i] * r[k] * (i == k ? 1 : 2);
                for (Tuint l = k; l < 3; ++l)
                {
                    if (N >= 3)
                        ret += D[MI3[i][k][l]] * r[i] * r[k] * r[l] * (i == l ? 1 : (i == k || k == l ? 3 : 6));
                    for (Tuint m = l; m < 3; ++m)
                    {
                        if (N >= 4)
                            ret += E[MI4[i][k][l][m]] * r[i] * r[k] * r[l] * r[m]
                                   * (i == m ? 1
                                             : (i == l || k == m
                                                    ? 4
                                                    : (i == k && l == m ? 6 : (i == k || k == l || l == m ? 12 : 24))));
                    }
                }
            }
        }
        return ret;
    }
#endif

    Vector<T, 3> calc_force(Vector<T, 3> r) const
    {
        r = -r;
        Vector<T, 3> F = { 0, 0, 0 };
        for (Tuint i = 0; i < 3; ++i)
        {
            if (N >= 1)
                F[i] += B[i];
            for (Tuint k = 0; k < 3; ++k)
            {
                if (N >= 2)
                    F[i] += 2 * (C[MI2[k][i]]) * r[k];

                for (Tuint l = 0; l < 3; ++l)
                {
                    if (N >= 3)
                        F[i] += 3 * D[MI3[i][k][l]] * r[k] * r[l];
                    for (Tuint m = 0; m < 3; ++m)
                    {
                        if (N >= 4)
                            F[i] += 4 * E[MI4[i][k][l][m]] * r[k] * r[l] * r[m];
                    }
                }
            }
        }
        return F;
    }
    template<class STREAM>
    friend STREAM& operator<<(STREAM& s, const multipole& m)
    {
        s << "multipole:\nA=" << m.A << endl;
        if (N >= 1)
            s << "B=" << m.B << endl;
        if (N >= 2)
            s << "C=" << m.C << endl;
        if (N >= 3)
            s << "D=" << m.D << endl;
        if (N >= 4)
            s << "E=" << m.E << endl;
        return s;
    }
};

template<class T, unsigned int max_multipole>
struct treenode
{
    using goopax_struct_type = T;
    template<typename X>
    using goopax_struct_changetype = treenode<typename goopax_struct_changetype<T, X>::type, max_multipole>;
    using uint_type = typename change_gpu_mode<unsigned int, T>::type;

    uint_type first_child;
    uint_type pbegin;
    uint_type pend;
    // uint_type parent;
    multipole<T, max_multipole> Mr;

    template<class STREAM>
    friend STREAM& operator<<(STREAM& s, const treenode& n)
    {
        s << "[first_child=" << n.first_child << ", pbegin=" << n.pbegin << ", pend=" << n.pend << endl
          << "Mr=" << n.Mr << endl
          << "]";
        return s;
    }
};

void myassert(bool b)
{
    assert(b);
}
void myassert(gpu_bool b)
{
    gpu_assert(b);
}

template<class T>
auto calc_sig_fast(const Vector<T, 3>& x, Tuint32_t)
{
    const Tuint max_depthbits = 32;
    using sig_t = typename gettype<T>::template change<Tuint32_t>::type;

    Vector<Tuint, 3> depth = { (max_depthbits + 2) / 3, (max_depthbits + 1) / 3, (max_depthbits) / 3 };
    sig_t sig = 0;
    {
        Vector<sig_t, 3> s;
        for (Tint k = 0; k < 3; ++k)
        {
            sig_t s = sig_t(abs(x[k]) * (1.0 / (halflen * pow(2.0, Tdouble(2 - k) / 3)) * (1 << (depth[k] - 1))));
            myassert(s < (1u << depth[k]));
            s = cond(x[k] > 0, s, (1 << (depth[k] - 1)) - 1 - s);
            if (depth[k] - 1 > 8)
                s = ((s & 0x0000ff00) << 16) | (s & 0x000000ff);
            if (depth[k] - 1 > 4)
                s = ((s & 0xf0f0f0f0) << 8) | (s & 0x0f0f0f0f);
            if (depth[k] - 1 > 2)
                s = ((s & 0xcccccccc) << 4) | (s & 0x33333333);
            if (depth[k] - 1 > 1)
                s = ((s & 0xaaaaaaaa) << 2) | (s & 0x55555555);
            sig |= s << (2 - (k + (3 * 1000 - max_depthbits)) % 3);
        }
        for (Tint k = 0; k < 3; ++k)
            sig |= sig_t(x[k] > 0) << (max_depthbits - 1 - k);
    }
    return sig;
}

template<class T>
auto calc_sig_fast(const Vector<T, 3>& x, Tuint64_t)
{
    const Tuint max_depthbits = 64;
    using sig_t = typename change_gpu_mode<Tuint64_t, T>::type;

    Vector<Tuint, 3> depth = { (max_depthbits + 2) / 3, (max_depthbits + 1) / 3, (max_depthbits) / 3 };
    sig_t sig = 0;
    {
        Vector<sig_t, 3> s;
        for (Tint k = 0; k < 3; ++k)
        {
            sig_t s = sig_t(abs(x[k])
                            * static_cast<T>(1.0 / (halflen * pow(2.0, Tdouble(2 - k) / 3)) * (1 << (depth[k] - 1))));
            myassert(s < (1u << depth[k]));
            s = cond(x[k] > 0, s, (1 << (depth[k] - 1)) - 1 - s);
            if (depth[k] - 1 > 16)
                s = (gpu_uint64(s & 0xffff0000) << 32) | (s & 0x0000ffff);
            if (depth[k] - 1 > 8)
                s = ((s & 0xff00ff00ff00ff00) << 16) | (s & 0x00ff00ff00ff00ff);
            if (depth[k] - 1 > 4)
                s = ((s & 0xf0f0f0f0f0f0f0f0) << 8) | (s & 0x0f0f0f0f0f0f0f0f);
            if (depth[k] - 1 > 2)
                s = ((s & 0xcccccccccccccccc) << 4) | (s & 0x3333333333333333);
            if (depth[k] - 1 > 1)
                s = ((s & 0xaaaaaaaaaaaaaaaa) << 2) | (s & 0x5555555555555555);
            sig |= s << (2 - (k + (3 * 1000 - max_depthbits)) % 3);
        }
        for (Tint k = 0; k < 3; ++k)
            sig |= sig_t(x[k] > 0) << (max_depthbits - 1 - k);
    }
    return sig;
}

template<class signature_t, class T>
auto calc_sig(const Vector<T, 3>& x, Tuint max_depthbits)
{
    using sig_t = typename change_gpu_mode<signature_t, T>::type;
    assert(max_depthbits >= 3);
    assert(max_depthbits <= get_size<sig_t>::value * 8);
    Vector<Tuint, 3> depth = { (max_depthbits + 2) / 3, (max_depthbits + 1) / 3, (max_depthbits) / 3 };

    sig_t ret = calc_sig_fast<T>(x, signature_t()) >> (get_size<sig_t>::value * 8 - max_depthbits);

    if (DEBUG1)
    {
        sig_t cmp = 0;
        {
            Vector<sig_t, 3> s;
            for (Tint k = 0; k < 3; ++k)
            {
                cmp |= sig_t(x[k] > 0) << (max_depthbits - 1 - k);
                s[k] = sig_t(abs(x[k])
                             * static_cast<T>(1.0 / (halflen * pow(2.0, Tdouble(2 - k) / 3)) * (1 << (depth[k] - 1))));
                s[k] = cond(x[k] > 0, s[k], (1 << (depth[k] - 1)) - 1 - s[k]);
            }
            {
                Tuint k = 0;
                auto depth2 = depth;
                for (Tsize_t dest = max_depthbits - 4; dest != Tsize_t(-1); --dest)
                {
                    cmp |= (s[k] & (1 << (depth2[k] - 2))) << (dest - (depth2[k] - 2));
                    --depth2[k];
                    k = (k + 1) % 3;
                }
            }
        }

        myassert(ret == cmp);
    }
    return ret;
}

template<typename FUNC>
gpu_uint find_particle_split(const resource<pair<signature_t, CTuint>>& particles,
                             const gpu_uint begin,
                             const gpu_uint end,
                             FUNC func)
{
    gpu_uint split = begin;
    gpu_if(end != begin)
    {
        gpu_uint de = (end - begin + 2) / 2;
        gpu_while(de > 1u)
        {
            gpu_uint checksplit = min(split + de, end);
            split = cond(func(particles[checksplit - 1].first), checksplit, split);
            de = (de + 1) / 2;
        }
        gpu_uint checksplit = min(split + de, end);
        split = cond(func(particles[checksplit - 1].first), checksplit, split);
    }
    return split;
}

template<class T>
struct cosmos_base
{
    // static constexpr unsigned int gs_use = 4096;
    static constexpr unsigned int ls_use = 64;
    // static constexpr unsigned int ng_use = gs_use / ls_use;

    const Tuint sub_bits = log2_exact(ls_use) / 2;

    using gpu_T = typename make_gpu<T>::type;
    buffer<Vector<T, 3>> x;
    buffer<Vector<T, 3>> v;
#if CALC_POTENTIAL
    buffer<T> potential;
#endif
    buffer<T> mass;
    buffer<Vector<T, 3>> tmp;
    buffer<T> tmps; // FIXME: Reduce memory.

    buffer<pair<signature_t, CTuint>> plist1;
    buffer<pair<signature_t, CTuint>> plist2;
    const Tsize_t treesize;
    static const unsigned int treecount_blocksize = 2; // FIXME: Increase to 2 or so.

    buffer<CTuint> blocksums;
    buffer<CTuint> bigblocksums;
    buffer<CTuint> numsubbuf;

    const Tuint tree_depthbits;

    virtual void make_tree() = 0;

    kernel<void(buffer<Vector<T, 3>>& x,
                buffer<Vector<T, 3>>& v, // FIXME: hard link.
                Tuint size,
                T dt)>
        movefunc;

    kernel<void(const buffer<Vector<T, 3>>& x, buffer<pair<signature_t, CTuint>>& plist, Tuint size)> sort1func;

    kernel<void(const buffer<Vector<T, 3>>& in,
                buffer<Vector<T, 3>>& out,
                const buffer<pair<signature_t, CTuint>>& plist,
                Tuint size)>
        apply_vec;

    kernel<void(const buffer<T>& in, buffer<T>& out, const buffer<pair<signature_t, CTuint>>& plist, Tuint size)>
        apply_scalar;

    radix_sort<signature_t> Radix;

    kernel<void(buffer<CTuint>& blocksums, buffer<CTuint>& bigblocksums, Tuint num_blocksums)> treecount2func;

    struct vicinity_data
    {
        using index_t = Tuint16_t;

        vector<index_t> update_list;
        vector<index_t> local_list;
        vector<index_t> access_list;
        Vector<Tuint, 3> sizevec;
        Tuint size;
        Vector<Tint, 3> maxvec;

        vicinity_data(T max_distfac)
        {
            maxvec = { 0, 0, 0 };
        refresh_maxvec:
            update_list.clear();
            local_list.clear();
            access_list.clear();

            vector<Vector<Tint, 3>> update_vec;
            vector<Vector<Tint, 3>> local_vec;
            Tint bits = MAX_BIGNODE_BITS();
            Vector<Tint, 3> bitvec = { (bits + 2) / 3, (bits + 0) / 3, (bits + 1) / 3 };

            Vector<Tint, 3> ac;
            for (ac[2] = -max_distfac * pow<1, 3>(2.0) * 4 - 5; ac[2] <= max_distfac * pow<1, 3>(2.0) * 4 + 5; ++ac[2])
                for (ac[1] = -max_distfac * pow<1, 3>(2.0) * 4 - 5; ac[1] <= max_distfac * pow<1, 3>(2.0) * 4 + 5;
                     ++ac[1])
                    for (ac[0] = -max_distfac * pow<1, 3>(2.0) * 4 - 5; ac[0] <= max_distfac * pow<1, 3>(2.0) * 4 + 5;
                         ++ac[0])
                    {
                        Vector<Tint, 3> ap = ac;
                        if (ac[0] >= 0)
                            ap[0] /= 2;
                        else
                            ap[0] = -(((-ac[0]) + 1) / 2);
                        Vector<Tint, 3> ap2 = { ac[2], (ac[0] + 1000000) / 2 - 500000, ac[1] };

                        Vector<Tdouble, 3> cc = { ac[0] * pow<2, 3>(2.0),
                                                  ac[1] * pow<4, 3>(2.0),
                                                  ac[2] * pow<3, 3>(2.0) };
                        Vector<Tdouble, 3> cp2 = { ap2[0] * pow<2, 3>(2.0),
                                                   ap2[1] * pow<4, 3>(2.0),
                                                   ap2[2] * pow<3, 3>(2.0) };
                        Vector<Tdouble, 3> halfboxc = { pow<-1, 3>(2.0), pow<1, 3>(2.0), 1.0 };
                        Vector<Tdouble, 3> mincp2;
                        Vector<Tdouble, 3> mincc;
                        for (Tuint k = 0; k < 3; ++k)
                        {
                            mincc[k] = max(abs(cc[k]) - 2 * halfboxc[k], 0.0);
                            mincp2[k] = max(abs(cp2[k]) - 2 * halfboxc[k], 0.0);
                        }

                        Tbool uc = (mincc.squaredNorm() < pow2(max_distfac * pow<-1, 3>(2.0)));
                        Tbool up2 = (mincp2.squaredNorm() < pow2(max_distfac * pow<-1, 3>(2.0)));
                        if (uc)
                        {
                            local_vec.push_back(ac);
                        }
                        if (up2 && !uc)
                        {
                            update_vec.push_back(ac);
                        }
                        if (uc && !up2)
                        {
                            abort();
                        }
                    }

            for (auto& n : update_vec)
            {
                for (Tuint k = 0; k < 3; ++k)
                {
                    maxvec[k] = max(maxvec[k], abs(n[k]));
                }
            }
            for (Tuint k = 0; k < 3; ++k)
                sizevec[k] = 2 * maxvec[k] + (1 << bitvec[k]);
            cout1 << "maxvec=" << maxvec << endl << "sizevec=" << sizevec << endl;

            if (POW2_SIZEVEC())
            {
                cout1 << "Increasing sizevec from " << sizevec;
                for (Tuint k : { 1, 2 })
                {
                    while ((sizevec[k] & (sizevec[k] - 1)) != 0)
                    {
                        ++sizevec[k];
                    }
                }
                cout1 << " to " << sizevec << endl;
            }
            size = sizevec[0] * sizevec[1] * sizevec[2];

            set<Tuint> access_set;
            set<Tuint> access_set_U;
            for (auto& n : update_vec)
            {
                auto tmp = n + maxvec;
                Vector<Tuint, 3> nu;
                for (Tuint k = 0; k < 3; ++k)
                    nu[k] = tmp[k];

                Tuint id = nu[0] + nu[1] * sizevec[0] + nu[2] * sizevec[0] * sizevec[1];
                update_list.push_back(id);
                Vector<Tuint, 3> sv;
                for (sv[2] = 0; sv[2] < (1u << bitvec[2]); ++sv[2])
                    for (sv[1] = 0; sv[1] < (1u << bitvec[1]); ++sv[1])
                        for (sv[0] = 0; sv[0] < (1u << bitvec[0]); ++sv[0])
                        {
                            Vector<Tuint, 3> nu2 = nu + sv;
                            Tuint id2 = nu2[0] + nu2[1] * sizevec[0] + nu2[2] * sizevec[0] * sizevec[1];
                            access_set.insert(id2);
                            access_set_U.insert(id2);
                        }
            }
            for (auto& n : local_vec)
            {
                auto tmp = n + maxvec;
                Vector<Tuint, 3> nu;
                for (Tuint k = 0; k < 3; ++k)
                    nu[k] = tmp[k];

                Tuint id = nu[0] + nu[1] * sizevec[0] + nu[2] * sizevec[0] * sizevec[1];
                local_list.push_back(id);
                Vector<Tuint, 3> sv;
                for (sv[2] = 0; sv[2] < (1u << bitvec[2]); ++sv[2])
                    for (sv[1] = 0; sv[1] < (1u << bitvec[1]); ++sv[1])
                        for (sv[0] = 0; sv[0] < (1u << bitvec[0]); ++sv[0])
                        {
                            Vector<Tuint, 3> nu2 = nu + sv;
                            Tuint id2 = nu2[0] + nu2[1] * sizevec[0] + nu2[2] * sizevec[0] * sizevec[1];
                            access_set.insert(id2);
                        }
            }

            auto maxvec_new = maxvec;
            set<Tuint> old_access_set;
            while (old_access_set.size() != access_set.size())
            {
                old_access_set = access_set;
                for (Tuint n : old_access_set)
                {
                    for (Tuint bignode_is_child1 : { 0, 1 })
                    {
                        Vector<Tuint, 3> pos = { n % sizevec[0],
                                                 (n / sizevec[0]) % sizevec[1],
                                                 n / sizevec[0] / sizevec[1] };
                        Vector<Tint, 3> localpos = reinterpret<Vector<Tint, 3>>(pos) - maxvec;
                        for (Tuint k = 0; k < 3; ++k)
                        {
                            maxvec_new[k] = max((Tint)maxvec_new[k], -localpos[k]);
                            maxvec_new[k] = max((Tint)maxvec_new[k], localpos[k] - (bitvec[k] - 1));
                        }
                        Vector<Tint, 3> parent_localpos = { Tint(pos[2] - maxvec[2]),
                                                            Tint((pos[0] - maxvec[0] + 1000000)) / 2 - 500000,
                                                            Tint(pos[1] - maxvec[1]) };
                        for (Tuint k = 0; k < 3; ++k)
                        {
                            if (bignode_is_child1 && ((MAX_BIGNODE_BITS() + 1) % 3 == 2 - k) && bits != 0)
                            {
                                parent_localpos[k] += (1u << (bitvec[k] - 1));
                            }
                        }
                        Vector<Tint, 3> parent_pos = parent_localpos + maxvec;
                        const Tuint parent_p =
                            (parent_pos[0] + parent_pos[1] * (sizevec[0]) + parent_pos[2] * (sizevec[0] * sizevec[1]));
                        access_set.insert(parent_p);
                    }
                }
            }
            cout1 << "maxvec=" << maxvec << endl << "maxvec_new=" << maxvec_new << endl;
            if (maxvec != maxvec_new)
            {
                maxvec = maxvec_new;
                goto refresh_maxvec;
            }

            for (auto& a : access_set)
            {
                access_list.push_back(a * 2 + (access_set_U.find(a) != access_set_U.end()));
            }

            if (VERB1)
            {
                cout << "update_list.size()=" << update_list.size() << endl
                     << "local_list.size()=" << local_list.size() << endl
                     << "access_list.size()=" << access_list.size() << endl;
                cout << "alloc size=" << sizevec[0] * sizevec[1] * sizevec[2] << endl;
                cout << "update_list=" << update_list << endl;
                cout << "local_list=" << local_list << endl;
                cout << "access_list=" << access_list << endl;
            }
            assert(update_list.size() == local_list.size());
        }
    };

    const vicinity_data vdata;
    buffer<typename vicinity_data::index_t> vicinity_update_buffer;
    buffer<typename vicinity_data::index_t> vicinity_local_buffer;
    buffer<typename vicinity_data::index_t> vicinity_access_buffer;

    kernel<void(const buffer<T>& potential, buffer<Vector<CTfloat, 4>>& color_gl, Tuint size)> extract_x_func;

    void step()
    {
        cout1 << "Moving." << endl;
        movefunc(x, v, x.size(), 0.5 * DT());
        cout1 << "Calculating force." << endl;
        this->make_tree();
        cout1 << "Moving." << endl;
        movefunc(x, v, x.size(), 0.5 * DT());
    }

    void make_IC(const char* filename = nullptr)
    {
        goopax_device device = x.get_device();
        size_t N = x.size();

        std::default_random_engine generator;
        std::normal_distribution<double> distribution;
        std::uniform_real_distribution<double> distribution2;

        if (filename)
        {
            v.fill({ 0, 0, 0 });
            cout << "Reading from file " << filename << endl;
#if !WITH_OPENCV
            throw std::runtime_error("Need opencv to read images");
#else
            cv::Mat image_color = cv::imread(filename);
            if (image_color.empty())
            {
                throw std::runtime_error("Failed to read image");
            }

            cv::Mat image_gray;
            cv::cvtColor(image_color, image_gray, cv::COLOR_BGR2GRAY);

            uint max_extent = max(image_gray.rows, image_gray.cols);
            Vector<double, 3> cm = { 0, 0, 0 };
            buffer_map cx(this->x);
            for (auto& r : cx)
            {
                // cout << "." << flush;
                while (true)
                {
                    for (auto& xx : r)
                    {
                        xx = distribution2(generator);
                    }
                    r[2] *= 0.1;
                    Vector<int, 3> ri = (r * max_extent).template cast<int>();
                    if (ri[0] < image_gray.cols && ri[1] < image_gray.rows)
                    {
                        uint8_t c = image_gray.at<uint8_t>(
                            { static_cast<int>(r[0] * max_extent), static_cast<int>(r[1] * max_extent) });
                        if (distribution2(generator) * 255 < c)
                        {
                            cm += r.template cast<double>();
                            break;
                        }
                    }
                }
            }
            cm /= N;
            for (auto& r : cx)
            {
                r -= cm.cast<Tfloat>();
            }
            double extent2 = 0;
            for (auto& r : cx)
            {
                extent2 += r.squaredNorm();
            }
            extent2 /= N;
            for (auto& r : cx)
            {
                r *= 0.5f / sqrt(extent2);
                r[1] *= -1;
            }
#endif
        }
        else
        {
            Tint MODE = 2;
            if (MODE == 2)
            {
                buffer_map x(this->x);
                buffer_map v(this->v);
                for (Tuint k = 0; k < N; ++k) // Setting the initial conditions:
                { // N particles of mass 1/N each are randomly placed in a sphere of radius 1
                    Vector<T, 3> xk;
                    Vector<T, 3> vk;
                    do
                    {
                        for (Tuint i = 0; i < 3; ++i)
                        {
                            xk[i] = distribution(generator) * 0.2;
                            vk[i] = distribution(generator) * 0.2;
                        }
                    } while (xk.squaredNorm() >= 1);
                    x[k] = xk;
                    vk += Vector<T, 3>({ -xk[1], xk[0], 0 }) / (Vector<T, 3>({ -xk[1], xk[0], 0 })).norm() * 0.4f
                          * min(xk.norm() * 10, (T)1);
                    if (k < N / 2)
                        vk = -vk;
                    v[k] = vk;
                    if (k < N / 2)
                    {
                        x[k] += Vector<T, 3>{ 0.8, 0.2, 0.0 };
                        v[k] += Vector<T, 3>{ -0.4, 0.0, 0.0 };
                    }
                    else
                    {
                        x[k] -= Vector<T, 3>{ 0.8, 0.2, 0.0 };
                        v[k] += Vector<T, 3>{ 0.4, 0.0, 0.0 };
                    }
                }
            }
            else if (MODE == 3)
            {
                buffer_map x(this->x);
                for (Tsize_t p = 0; p < this->x.size(); ++p)
                {
                    for (Tint k = 0; k < 3; ++k)
                    {
                        do
                        {
                            x[p][k] = distribution(generator);
                        } while (abs(x[p][k]) >= 1);
                    }
                }
                v.fill({ 0, 0, 0 });
            }
        }
        mass.fill(1.0 / N);
    }

    void precision_test()
    {
        cout << "Doing precision test" << endl;
        goopax_device device = x.get_device();

        kernel verify(device,
                      [](const resource<Vector<T, 3>>& x,
                         const resource<T>& mass,
                         const resource<Vector<T, 3>>& force,
#if CALC_POTENTIAL
                         gather_add<TDOUBLE>& poterr,
                         const resource<T>& potential,
#endif
                         gpu_uint pnum,
                         gpu_uint np) -> gather_add<double> {
                          GPU_DOUBLE ret = 0;
#if CALC_POTENTIAL
                          poterr = 0;
#endif
                          gpu_for_global(0, np, [&](gpu_uint p) {
                              gpu_uint a = gpu_uint(gpu_uint64(pnum) * p / np);
                              Vector<GPU_DOUBLE, 3> F = { 0, 0, 0 };
                              GPU_DOUBLE P = 0;
                              gpu_for(0, pnum, [&](gpu_uint b) {
                                  auto distf = x[b] - x[a];
                                  Vector<GPU_DOUBLE, 3> dist;
                                  for (Tuint k = 0; k < 3; ++k)
                                  {
                                      dist[k] = (GPU_DOUBLE)(distf[k]);
                                  }
                                  F += static_cast<GPU_DOUBLE>(mass[b]) * dist * pow<-3, 2>(dist.squaredNorm() + 1E-20);
                                  P += cond(b == a, 0., -mass[b] * pow<-1, 2>(dist.squaredNorm() + 1E-20));
                              });
                              ret += (force[a].template cast<GPU_DOUBLE>() * (1.0 / DT()) - F).squaredNorm();
#if CALC_POTENTIAL
                              poterr += pow2(potential[a] - P);
#endif
                          });
                          return ret;
                      });

        vector<Tdouble> tottimevec;
        for (Tuint k = 0; k < 5; ++k)
        {
            v.fill({ 0, 0, 0 });

            device.wait_all();

            auto t0 = steady_clock::now();
            make_tree();
            device.wait_all();
            auto t1 = steady_clock::now();

            tottimevec.push_back(duration<double>(t1 - t0).count());
        }
        std::sort(tottimevec.begin(), tottimevec.end());
        cout << "tottime=" << tottimevec << endl;

        const Tuint np = min(x.size(), (Tuint)100);

        goopax_future<Tdouble> poterr;
        Tdouble tot = verify(x,
                             mass,
                             v,
#if CALC_POTENTIAL
                             poterr,
                             potential,
#endif
                             x.size(),
                             np)
                          .get();
        cout << "err=" << sqrt(tot / np) << ", poterr=" << sqrt(poterr.get() / np) << endl;
    }

    cosmos_base(goopax_device device, Tsize_t N, Tdouble max_distfac)
        : x(device, N)
        , v(device, N)
        ,
#if CALC_POTENTIAL
        potential(device, N)
        ,
#endif
        mass(device, N)
        , tmp(device, N)
        , tmps(device, N)
        ,

        plist1(device, N)
        , plist2(device, N)
        , treesize(0.3 * N + 1000)
        , blocksums(device, (treesize + treecount_blocksize - 1) / treecount_blocksize)
        , numsubbuf(device, 1)
        , tree_depthbits(MAX_DEPTH() - MAX_BIGNODE_BITS() - log2_exact(ls_use) / 2)
        , Radix(device)
        , vdata(max_distfac)
        , vicinity_update_buffer(device, vector(vdata.update_list))
        , vicinity_local_buffer(device, vector(vdata.local_list))
        , vicinity_access_buffer(device, vector(vdata.access_list))

    {
        make_IC();

        movefunc.assign(device,
                        [](resource<Vector<T, 3>>& x,
                           resource<Vector<T, 3>>& v, // FIXME: hard link.
                           gpu_uint size,
                           gpu_T dt) {
                            gpu_for_global(0, size, [&](gpu_uint k) {
                                x[k] += Vector<gpu_T, 3>(v[k]) * dt;

                                gpu_bool ok = true;
                                for (Tuint i = 0; i < 3; ++i)
                                {
                                    ok = ok && (abs(x[k][i]) <= halflen);
                                    x[k][i] = max(x[k][i], -halflen);
                                    x[k][i] = min(x[k][i], halflen);
                                }
                                gpu_if(!ok)
                                {
                                    x[k] *= (gpu_T)0.99f;
                                    v[k] = { 0, 0, 0 };
                                }
                            });
                        });

        sort1func.assign(
            device, [](const resource<Vector<T, 3>>& x, resource<pair<signature_t, CTuint>>& plist, gpu_uint size) {
                gpu_for_global(0, size, [&](gpu_uint k) {
                    const auto sig = calc_sig<signature_t>(x[k], MAX_DEPTH());
                    plist[k] = make_pair(sig, k);
                });
            });

        apply_vec.assign(
            device,
            [](const resource<Vector<T, 3>>& in,
               resource<Vector<T, 3>>& out,
               const resource<pair<signature_t, CTuint>>& plist,
               gpu_uint size) { gpu_for_global(0, size, [&](gpu_uint k) { out[k] = in[plist[k].second]; }); });

        apply_scalar.assign(
            device,
            [](const resource<T>& in,
               resource<T>& out,
               const resource<pair<signature_t, CTuint>>& plist,
               gpu_uint size) { gpu_for_global(0, size, [&](gpu_uint k) { out[k] = in[plist[k].second]; }); });

        treecount2func.assign(
            device, [](resource<CTuint>& blocksums, resource<CTuint>& bigblocksums, gpu_uint num_blocksums) {
                assert(global_size() % treecount_blocksize == 0);
                gpu_for_global(0, num_blocksums, (global_size() / treecount_blocksize), [&](gpu_uint offset) {
                    gpu_uint sum = 0;
                    gpu_for(offset, min(offset + global_size() / treecount_blocksize, num_blocksums), [&](gpu_uint k) {
                        gpu_uint oldsum = sum;
                        sum += blocksums[k];
                        blocksums[k] = oldsum; // FIXME: Can this me optimized? blocking? Is this done automatically?
                    });
                    bigblocksums[offset * treecount_blocksize / global_size()] = sum;
                });
            });

        bigblocksums.assign(device, (treesize + treecount2func.global_size() - 1) / treecount2func.global_size());

        extract_x_func.assign(device,
                              [](const resource<T>& potential, resource<Vector<CTfloat, 4>>& color_gl, gpu_uint size) {
                                  gpu_for_global(0, size, [&](gpu_uint k) { color_gl[k] = color(potential[k]); });
                              });
    }
};

template<class T>
const unsigned int cosmos_base<T>::treecount_blocksize;

template<class T, unsigned int max_multipole>
struct cosmos : public cosmos_base<T>
{
    using gpu_T = typename gettype<T>::gpu;
    using cosmos_base<T>::treecount_blocksize;
    using cosmos_base<T>::tree_depthbits;
    using cosmos_base<T>::vdata;
    using typename cosmos_base<T>::vicinity_data;
    using cosmos_base<T>::x;
    using cosmos_base<T>::v;
    using cosmos_base<T>::mass;
    using cosmos_base<T>::potential;
    using cosmos_base<T>::plist1;
    using cosmos_base<T>::plist2;
    using cosmos_base<T>::tmp;
    using cosmos_base<T>::tmps;
    using cosmos_base<T>::blocksums;
    using cosmos_base<T>::bigblocksums;
    using cosmos_base<T>::numsubbuf;

    buffer<treenode<T, max_multipole>> tree;
    buffer<treenode<T, max_multipole>> fill3;

    array<kernel<void(buffer<treenode<T, max_multipole>>& tree,
                      Tuint treebegin,
                      Tuint treeend,
                      // const gpu_uint max_super_particles,
                      buffer<CTuint>& blocksums)>,
          2>
        treecount1;

    array<kernel<void(buffer<treenode<T, max_multipole>>& tree,
                      const buffer<pair<signature_t, CTuint>>& particles,
                      Tuint treeoffset,
                      Tuint treesize,
                      Tuint tree_maxsize,
                      Tuint depth,
                      T halflen_sublevel,
                      const buffer<CTuint>& blocksums,
                      const buffer<CTuint>& bigblocksums,
                      buffer<CTuint>& numsub)>,
          3>
        treecount3;

    array<kernel<void(const buffer<treenode<T, max_multipole>>& tree,
                      const buffer<Vector<T, 3>>& x,
                      const Tuint begin,
                      const Tuint end,
                      const Vector<T, 3> halflen_level)>,
          3>
        treetest;

    array<array<kernel<void(buffer<treenode<T, max_multipole>>& tree,
                            const buffer<Vector<T, 3>>& xvec,
                            const buffer<T>& massvec,
                            Tuint treebegin,
                            Tuint treeend,
                            T level_halflen)>,
                2>,
          3>
        upwards;

    template<class U>
    struct local_treenode
    {
        GOOPAX_PREPARE_STRUCT2(local_treenode, U)
        using uint_type = typename change_gpu_mode<unsigned int, U>::type;

        uint_type pbegin;
        uint_type pend;
        multipole<U, max_multipole> Mr;

        template<class STREAM>
        friend STREAM& operator<<(STREAM& s, const local_treenode& n)
        {
            s << "[pbegin=" << n.pbegin << ", pend=" << n.pend << ", Mr=" << n.Mr << "]";
            return s;
        }
    };

    template<class U>
    struct vicinity_treenode
    {
        GOOPAX_PREPARE_STRUCT2(vicinity_treenode, U)
        using uint_type = typename change_gpu_mode<unsigned int, U>::type;

        multipole<U, max_multipole> Mr;
        uint_type first_child;
        uint_type pbegin;
        uint_type pend;
        template<class STREAM>
        friend STREAM& operator<<(STREAM& s, const vicinity_treenode& n)
        {
            s << "[first_child=" << n.first_child << ", pbegin=" << n.pbegin << ", pend=" << n.pend;
            s << ", Mr=" << n.Mr << "]";
            return s;
        }
    };

    buffer<local_treenode<T>> local_tree;
    buffer<vicinity_treenode<T>> vicinity_tree;

    struct coordinates
    {
        const cosmos_base<T>& Cosmos;
        Vector<gpu_T, 3> shiftvec = { gpu_T(pow<2, 3>(2.0) * halflen),
                                      (gpu_T)pow<4, 3>(2.0) * halflen,
                                      gpu_T(pow<3, 3>(2.0) * halflen) };

        void move_up()
        {
            shiftvec[0] *= 2;
            shiftvec = rot(shiftvec, -1);
        }
        void move_down()
        {
            shiftvec = rot(shiftvec);
            shiftvec[0] /= 2;
        }
        Vector<gpu_T, 3> getpos_r(const Vector<gpu_int, 3>& u) const
        {
            Tint bits = MAX_BIGNODE_BITS();
            Vector<Tint, 3> bitvec = { (bits + 2) / 3, (bits + 0) / 3, (bits + 1) / 3 };
            Vector<gpu_T, 3> sv2;
            for (Tuint k = 0; k < 3; ++k)
            {
                sv2[k] = u[k] - ((1 << bitvec[k]) - 1) * 0.5f;
            }

            Vector<gpu_T, 3> ret;
            for (Tuint k = 0; k < 3; ++k)
                ret[k] = shiftvec[k] * sv2[k];
            return ret;
        }

        Vector<gpu_T, 3> getsubshift_r(gpu_uint sub) const
        {
            coordinates D = *this;
            Vector<gpu_T, 3> ret = { 0, 0, 0 };
            for (Tint k = Cosmos.sub_bits - 1; k >= 0; --k)
            {
                D.move_down();
                ret[(Cosmos.sub_bits - k) % 3] +=
                    cond((sub & (1 << k)) != 0, D.shiftvec[0] * 0.5f, -D.shiftvec[0] * 0.5f);
            }
            return ret;
        }

        coordinates(const cosmos_base<T>& Cosmos0)
            : Cosmos(Cosmos0)
        {
            for (Tuint k = 0; k < MAX_BIGNODE_BITS() - 1; ++k)
            {
                shiftvec = rot(shiftvec);
                shiftvec[0] /= 2;
            }
        }
    };

    kernel<void(buffer<treenode<T, max_multipole>>& tree,
                buffer<local_treenode<T>>& local_tree,
                buffer<vicinity_treenode<T>>& vicinity_tree,
                const buffer<Vector<T, 3>>& x,
                const buffer<pair<signature_t, CTuint>>& plist,
                const Tuint num_particles,
                const buffer<T>& mass,
#if CALC_POTENTIAL
                buffer<T>& potential,
#endif
                buffer<Vector<T, 3>>& v)>
        downwards;

    virtual void make_tree() final
    {
        this->sort1func(x, plist1, x.size());
        this->Radix(plist1, plist2, MAX_DEPTH());

        this->apply_vec(x, tmp, plist1, plist1.size());
        swap(x, tmp);
        this->apply_vec(v, tmp, plist1, plist1.size());
        swap(v, tmp);
        this->apply_scalar(mass, tmps, plist1, plist1.size());
        swap(mass, tmps);

        static vector<pair<Tuint, Tuint>> treerange;
        treerange.clear();
        Tuint treesize = 1;
        Tuint treeoffset = 2;

#if WITH_TIMINGS
        x.get_device().wait_all();
        auto t0 = steady_clock::now();
#endif

        {
            tree.copy(fill3, 3, 0, 0);

            for (Tuint depth = 0; depth < MAX_DEPTH(); ++depth)
            {
                treerange.push_back(make_pair(treeoffset, treeoffset + treesize));
                cout1 << "\ndepth " << depth << ": tree[" << treeoffset << "..." << treeoffset + treesize << "]=";
                if (depth == MAX_DEPTH() - 1)
                    break;

                this->treecount1[depth < this->sub_bits + MAX_BIGNODE_BITS()](
                    tree, treeoffset, treeoffset + treesize, blocksums);

                cout1 << "after treecount1:\nblocksums=" << blocksums << endl;
                this->treecount2func(
                    blocksums, bigblocksums, (treesize + treecount_blocksize - 1) / treecount_blocksize);
                cout1 << "after treecount2:\nblocksums=" << blocksums << endl;
                cout1 << "bigblocksums=" << bigblocksums << endl;

                treecount3[depth % 3](tree,
                                      plist1,
                                      treeoffset,
                                      treesize,
                                      tree.size(),
                                      MAX_DEPTH() - depth - 1,
                                      halflen * pow(2.0, (-1 - Tint(depth)) / 3.0),
                                      blocksums,
                                      bigblocksums,
                                      numsubbuf);

                const_buffer_map<Tuint> numsubbuf(this->numsubbuf);
                const Tuint num_sub = numsubbuf[0];
                cout1 << "num_sub=" << num_sub << endl;

                assert(treeoffset + treesize + num_sub <= tree.size()); // FIXME: Make dynamic.

                Vector<T, 3> boxsize;
                for (Tuint k = 0; k < 3; ++k)
                {
                    boxsize[k] = halflen * (pow(2.0, -Tint(depth + 2 - k) / 3 + (2.0 - k) / 3.0) + 1E-7);
                }
                cout1 << "boxsize=" << boxsize << endl;

#ifndef NDEBUG
                static Tuint testcount = 0;

                if (testcount++ % 1024 == 0)
                {
                    cout << "treetest" << endl;
                    treetest[depth % 3](tree, x, treeoffset, treeoffset + treesize, boxsize);
                }
#endif

                treeoffset += treesize;
                treesize = num_sub;

                if (num_sub == 0)
                    break;
            }
        }

#if WITH_TIMINGS
        x.get_device().wait_all();
        auto t1 = steady_clock::now();
#endif

        {
            Tdouble level_halflen = halflen * pow(2.0, -1.0 / 3 * treerange.size());
            for (Tuint depth = treerange.size() - 1; depth != Tuint(-1); --depth)
            {
                upwards[(3000000 + depth - 1 - this->sub_bits) % 3][depth == treerange.size() - 1](
                    tree, x, mass, treerange[depth].first, treerange[depth].second, level_halflen);
                level_halflen *= pow(2.0, 1.0 / 3);
            }
        }

#if WITH_TIMINGS
        x.get_device().wait_all();
        auto t2 = steady_clock::now();
#endif

        if (treerange.size() > MAX_DEPTH())
        {
            cerr << "treerange.size()=" << treerange.size() << " > MAX_DEPTH=" << MAX_DEPTH() << endl;
            throw std::runtime_error("MAX_DEPTH exceeded");
        }

        downwards(tree,
                  local_tree,
                  vicinity_tree,
                  x,
                  plist1,
                  plist1.size(),
                  mass,
#if CALC_POTENTIAL
                  potential,
#endif
                  v);

#if WITH_TIMINGS
        x.get_device().wait_all();
        auto t3 = steady_clock::now();

        cout << "treecount: " << duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms" << endl;
        cout << "upwards: " << duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms" << endl;
        cout << "downwards: " << duration_cast<std::chrono::milliseconds>(t3 - t2).count() << " ms" << endl;
#endif
    }

    cosmos(goopax_device device, Tsize_t N, Tdouble max_distfac)
        : cosmos_base<T>(device, N, max_distfac)
        , tree(device, this->treesize)
        , fill3(device, 3)
    {

        {
            buffer_map fill3(this->fill3);
            for (Tuint k : { 0, 1 })
            {
                fill3[k].pbegin = 0;
                fill3[k].pend = 0;
                fill3[k].first_child = 0;
                fill3[k].Mr.B = { numeric_limits<T>::infinity(),
                                  numeric_limits<T>::infinity(),
                                  numeric_limits<T>::infinity() };
            }

            fill3[2].pbegin = 0;
            fill3[2].pend = plist1.size();
            fill3[2].Mr = multipole<T, max_multipole>::from_particle({ 0, 0, 0 }, 0);
        }

        for (bool is_top : { false, true })
        {
            treecount1[is_top].assign(
                device,
                [is_top](resource<treenode<T, max_multipole>>& tree,
                         gpu_uint treebegin,
                         gpu_uint treeend,
                         // const gpu_uint max_super_particles,
                         resource<CTuint>& blocksums) {
                    array<gpu_bool, treecount_blocksize> has_children_vec;
                    gpu_for(treecount_blocksize * local_size() * group_id(),
                            treeend - treebegin,
                            global_size() * treecount_blocksize,
                            [&](gpu_uint offset) {
                                gpu_uint sum = 0;
                                for (unsigned int k = 0; k < treecount_blocksize; ++k)
                                {
                                    gpu_uint rawpos = offset + treebegin + local_id() * treecount_blocksize + k;
                                    gpu_uint pos = min(rawpos, treeend - 1);
                                    gpu_bool has_children =
                                        (tree[pos].pend - tree[pos].pbegin > MAX_NODESIZE()) || is_top;
                                    has_children = has_children && (rawpos < treeend);
                                    sum += (gpu_uint)has_children;
                                    has_children_vec[k] = has_children;
                                }
                                blocksums[offset / treecount_blocksize + local_id()] = sum * 2;
                                gpu_uint toffset = 0;
                                for (Tuint k = 0; k < treecount_blocksize; ++k)
                                {
                                    gpu_uint pos = offset + treebegin + local_id() * treecount_blocksize + k;
                                    gpu_if(pos < treeend || (k < treecount_blocksize - 1))
                                    {
                                        tree[pos].first_child = cond(has_children_vec[k], treeend + toffset, 0u);
                                        toffset += 2 * (gpu_uint)has_children_vec[k];
                                    }
                                }
                            });
                });
        }

        for (unsigned int mod3 = 0; mod3 < 3; ++mod3)
        {
            treecount3[mod3].assign(
                device,
                [mod3](resource<treenode<T, max_multipole>>& tree,
                       const resource<pair<signature_t, CTuint>>& particles,
                       gpu_uint treeoffset,
                       gpu_uint treesize,
                       gpu_uint tree_maxsize,
                       gpu_uint depth,
                       gpu_T halflen_sublevel,
                       const resource<CTuint>& blocksums,
                       const resource<CTuint>& bigblocksums,
                       resource<CTuint>& numsub) {
                    gpu_uint offsetsum = 0;
                    gpu_for_global(0, treesize, [&](gpu_uint k) {
                        tree[treeoffset + k].first_child =
                            cond((tree[treeoffset + k].first_child != 0),
                                 tree[treeoffset + k].first_child + offsetsum + blocksums[k / treecount_blocksize],
                                 0u);
                        const treenode<gpu_T, max_multipole> n = tree[treeoffset + k];
                        gpu_if(n.first_child != 0)
                        {
                            const gpu_uint end =
                                find_particle_split(particles, n.pbegin, n.pend, [&](gpu_signature_t id) {
                                    return ((id & (gpu_signature_t(1u) << depth)) == 0);
                                });

                            gpu_bool ok = (n.first_child + 1 < tree_maxsize);
                            gpu_assert(ok);
                            for (Tuint childnum : { 0, 1 })
                            {
                                auto& c = tree[cond(ok, n.first_child + childnum, treeoffset + k)];
                                Vector<gpu_T, 3> center = n.Mr.B;
                                for (Tuint dir : { 0, 1, 2 })
                                    if (mod3 == dir)
                                        center[dir] += (childnum == 0 ? -halflen_sublevel : halflen_sublevel);
                                c.Mr.B = center;
                                if (childnum == 0)
                                {
                                    c.pbegin = n.pbegin;
                                    c.pend = end;
                                }
                                else
                                {
                                    c.pbegin = end;
                                    c.pend = n.pend;
                                }
                            }
                        }
                        offsetsum += bigblocksums[k / global_size()];
                    });
                    gpu_if(global_id() == 0) numsub[0] = offsetsum;
                },
                this->treecount2func.local_size(),
                this->treecount2func.global_size());

            treetest[mod3].assign(device,
                                  [mod3](const resource<treenode<T, max_multipole>>& tree,
                                         const resource<Vector<T, 3>>& x,
                                         const gpu_uint begin,
                                         const gpu_uint end,
                                         const Vector<gpu_T, 3> halflen_level) {
                                      gpu_for_global(begin, end, [&](gpu_uint k) {
                                          auto n = tree[k];
                                          gpu_for(n.pbegin, n.pend, [&](gpu_uint p) {
                                              for (Tuint i = 0; i < 3; ++i)
                                              {
                                                  gpu_assert(abs(x[p][i] - n.Mr.B[i]) <= halflen_level[i]);
                                              }
                                          });
                                      });
                                  });

            for (bool is_bottom : { false, true })
            {
                upwards[mod3][is_bottom].assign(
                    device,
                    [is_bottom, mod3, this](resource<treenode<T, max_multipole>>& tree,
                                            const resource<Vector<T, 3>>& xvec,
                                            const resource<T>& massvec,
                                            gpu_uint treebegin,
                                            gpu_uint treeend,
                                            gpu_T level_halflen) {
                        gpu_for_global(treebegin, treeend, [&](gpu_uint t) {
                            const gpu_bool is_pnode = (is_bottom || tree[t].first_child == 0);

                            multipole<gpu_T, max_multipole> Msum_r =
                                multipole<T, max_multipole>::from_particle({ 0, 0, 0 }, 0);

                            gpu_for(tree[t].pbegin, cond(is_pnode, tree[t].pend, tree[t].pbegin), [&](gpu_uint p) {
                                Msum_r += multipole<gpu_T, max_multipole>::from_particle(
                                              xvec[p] - tree[t].Mr.B, massvec[p], (gpu_uint)p)
                                              .rot(mod3);
                            });
                            gpu_for(0, cond(is_pnode, 0u, 2u), [&](gpu_uint child) {
                                const gpu_uint child_id = tree[t].first_child + child;
                                const multipole<gpu_T, max_multipole> Mcr = tree[child_id].Mr;
                                Vector<gpu_T, 3> shift_r = { level_halflen * (1 - gpu_int(2 * child)), 0, 0 };
                                multipole<gpu_T, max_multipole> Mr =
                                    Mcr.rot(-1).shift_ext(rot(shift_r, Tint(-1 - this->sub_bits)));
                                Msum_r += Mr;
                            });

                            tree[t].Mr = Msum_r;
                        });
                    });
            }
        }

        downwards.assign(
            device,
            [this](resource<treenode<T, max_multipole>>& tree,
                   resource<local_treenode<T>>& local_tree,
                   resource<vicinity_treenode<T>>& vicinity_tree,
                   const resource<Vector<T, 3>>& x,
                   const resource<pair<signature_t, CTuint>>& plist,
                   const gpu_uint num_particles,
                   const resource<T>& mass,
#if CALC_POTENTIAL
                   resource<T>& potential,
#endif
                   resource<Vector<T, 3>>& v) {
                vector<gpu_uint> COUNT(13, 0);
                using bignodeshift_and_t = typename std::conditional<sizeof(T) == 8, gpu_uint64, gpu_uint>::type;

                const resource<typename vicinity_data::index_t> vicinity_update_list_res(this->vicinity_update_buffer);
                const resource<typename vicinity_data::index_t> vicinity_local_list_res(this->vicinity_local_buffer);
                const resource<typename vicinity_data::index_t> vicinity_access_list_res(this->vicinity_access_buffer);
                local_mem<typename vicinity_data::index_t> vicinity_update_list(vdata.update_list.size());
                local_mem<typename vicinity_data::index_t> vicinity_local_list(vdata.local_list.size());
                local_mem<typename vicinity_data::index_t> vicinity_access_list(vdata.access_list.size());
                vicinity_update_list.copy(vicinity_update_list_res);
                vicinity_local_list.copy(vicinity_local_list_res);
                vicinity_access_list.copy(vicinity_access_list_res);

                const Tuint num_sub = 1 << this->sub_bits;
                assert(local_size() == pow2(num_sub));

                assert(local_size() % num_sub == 0);
                const gpu_uint sub = local_id() % num_sub;

                const Tuint local_offset = vdata.maxvec[0] + vdata.maxvec[1] * vdata.sizevec[0]
                                           + vdata.maxvec[2] * vdata.sizevec[1] * vdata.sizevec[0];
                const gpu_uint pbegin = gpu_uint(gpu_uint64(num_particles) * group_id() / num_groups());
                const gpu_uint pend = gpu_uint(gpu_uint64(num_particles) * (group_id() + 1) / num_groups());
                const Vector<Tuint, 3> bitvec = { (MAX_BIGNODE_BITS() + 2) / 3,
                                                  (MAX_BIGNODE_BITS() + 0) / 3,
                                                  (MAX_BIGNODE_BITS() + 1) / 3 };

                gpu_for_local(0, num_sub * (1 << (MAX_BIGNODE_BITS() - 1)), [&](gpu_uint sb) {
                    gpu_uint sub = sb % num_sub;
                    gpu_uint node = sb / num_sub;
                    const gpu_uint src = 1 + (1 << (this->sub_bits + MAX_BIGNODE_BITS() - 1)) + sb;

                    Vector<gpu_uint, 3> vpos = { 0, 0, 0 };
                    for (Tuint d = 0; d < MAX_BIGNODE_BITS(); ++d)
                    {
                        vpos = { vpos[1] * 2 | ((node >> (MAX_BIGNODE_BITS() - 1 - d)) & 1), vpos[2], vpos[0] };
                    }
                    gpu_uint lposl = vpos[0] | (vpos[1] << (bitvec[0])) | (vpos[2] << (bitvec[0] + bitvec[1]));
                    vpos += vdata.maxvec.template cast<gpu_uint>();
                    gpu_uint vposl =
                        vpos[0] + vpos[1] * vdata.sizevec[0] + vpos[2] * vdata.sizevec[0] * vdata.sizevec[1];

                    auto& lt = local_tree[group_id() * tree_depthbits * (1 << MAX_BIGNODE_BITS()) * num_sub
                                          + lposl * num_sub + sub];
                    lt.Mr = multipole<T, max_multipole>::from_particle({ 0, 0, 0 }, 0);
                    gpu_uint begin = max(min(tree[src].pbegin, pend), pbegin);
                    gpu_uint end = max(min(tree[src].pend, pend), pbegin);
                    lt.pbegin = begin;
                    lt.pend = end;
                    auto& vt = vicinity_tree[(group_id() * tree_depthbits * vdata.size + vposl) * num_sub + sub];
                    vt.Mr = tree[src].Mr;
                    vt.first_child = tree[src].first_child;
                    vt.pbegin = tree[src].pbegin;
                    vt.pend = tree[src].pend;
                });
                local_tree.barrier();
                vicinity_tree.barrier();
                gpu_signature_t id_bignode = 0;
                gpu_uint depth_bm = 1;
                gpu_uint child_mod3 = MAX_BIGNODE_BITS() - 1;
                bignodeshift_and_t bignodeshift_and = 0;
                bignodeshift_and -= 2;

                coordinates COORDS(*this);
                Vector<gpu_T, 3> bignode_center_r = { 0, 0, 0 };

                gpu_while((depth_bm >= 2u || id_bignode == 0) && depth_bm < tree_depthbits)
                {
                    ++COUNT[0];
                    const gpu_uint vicinity_offset = group_id() * tree_depthbits * vdata.size + depth_bm * vdata.size;
                    gpu_bool bignode_is_child1 = ((id_bignode & 1u) != 0);

                    gpu_uint totnum_particles = 0;
                    gpu_for(local_id() / num_sub, vdata.access_list.size(), local_size() / num_sub, [&](gpu_uint va) {
                        ++COUNT[1];
                        // FIXME: Need to calculate M for all in access list (local/update)?
                        const gpu_uint v = vicinity_access_list[va] / 2;
                        const gpu_bool is_U = gpu_bool(vicinity_access_list[va] & 1);

                        Vector<gpu_int, 3> pos = { v % vdata.sizevec[0],
                                                   (v / vdata.sizevec[0]) % vdata.sizevec[1],
                                                   v / vdata.sizevec[0] / vdata.sizevec[1] }; // FIXME: Make faster.
                        Vector<gpu_int, 3> localpos = pos - vdata.maxvec.template cast<gpu_int>();
                        assert(vdata.maxvec[0] < 500);
                        Vector<gpu_int, 3> parent_localpos = { pos[2] - vdata.maxvec[2],
                                                               (pos[0] - vdata.maxvec[0] + 1000) / 2 - 500,
                                                               pos[1] - vdata.maxvec[1] };
                        if (MAX_BIGNODE_BITS() != 0)
                            for (Tuint k = 0; k < 3; ++k)
                            {
                                parent_localpos[k] = cond(bignode_is_child1 && ((MAX_BIGNODE_BITS() + 1) % 3 == 2 - k),
                                                          parent_localpos[k] + (1 << (bitvec[k] - 1)),
                                                          parent_localpos[k]);
                            }
                        Vector<gpu_uint, 3> parent_pos =
                            (parent_localpos + vdata.maxvec.template cast<gpu_int>()).template cast<gpu_uint>();
                        const gpu_uint parent_p = parent_pos[0] + parent_pos[1] * (vdata.sizevec[0])
                                                  + parent_pos[2] * (vdata.sizevec[0] * vdata.sizevec[1]);

                        {
                            gpu_uint childnum;
                            if ((vdata.maxvec[0] & 1) == 0)
                                childnum = v & 1;
                            else
                                childnum = 1 - (v & 1);
                            if (MAX_BIGNODE_BITS() == 0)
                                childnum = gpu_uint(bignode_is_child1);

                            const gpu_uint parent_sub = (sub >> 1) | ((childnum) << (this->sub_bits - 1));
                            const gpu_uint vt_parent_p = vicinity_offset - vdata.size + parent_p;

                            vicinity_treenode<gpu_T> vt_child;
                            const Vector<gpu_T, 3> vt_child_center_r =
                                bignode_center_r + COORDS.getpos_r(localpos) + COORDS.getsubshift_r(sub);

                            gpu_if(vicinity_tree[vt_parent_p * num_sub + parent_sub].first_child != 0)
                            {
                                ++COUNT[2];
                                const gpu_uint fc = vicinity_tree[vt_parent_p * num_sub + parent_sub].first_child;

                                vt_child.first_child = tree[fc + (sub & 1)].first_child;
                                vt_child.pbegin = tree[fc + (sub & 1)].pbegin;
                                vt_child.pend = tree[fc + (sub & 1)].pend;
                                vt_child.Mr = tree[fc + (sub & 1)].Mr;
                            }

                            gpu_if(vicinity_tree[vt_parent_p * num_sub + parent_sub].first_child == 0)
                            {
                                ++COUNT[3];
                                const gpu_uint vtp_pb = vicinity_tree[vt_parent_p * num_sub + parent_sub].pbegin;
                                const gpu_uint vtp_pe = vicinity_tree[vt_parent_p * num_sub + parent_sub].pend;
                                const gpu_signature_t bitmask =
                                    (gpu_signature_t(1)
                                     << (MAX_DEPTH() - depth_bm - MAX_BIGNODE_BITS() - this->sub_bits + 1));
                                const gpu_uint end = find_particle_split(
                                    plist, vtp_pb, vtp_pe, [&](gpu_signature_t id) { return ((id & bitmask) == 0); });
                                vt_child.pbegin = cond((sub & 1) == 0, vtp_pb, end);
                                vt_child.pend = cond((sub & 1) == 0, end, vtp_pe);
                                for (Tuint mod3 = 0; mod3 < 3; ++mod3)
                                {
                                    gpu_if(child_mod3 == mod3)
                                    {
                                        vt_child.Mr = multipole<T, max_multipole>::from_particle({ 0, 0, 0 }, 0);

                                        gpu_for(vt_child.pbegin, vt_child.pend, [&](gpu_uint p) {
                                            ++COUNT[4];
                                            vt_child.Mr += multipole<gpu_T, max_multipole>::from_particle(
                                                rot(x[p], mod3) - vt_child_center_r, mass[p], p);
                                        });
                                    }
                                }
                                vt_child.first_child = 0;
                            }
                            totnum_particles += vt_child.pend - vt_child.pbegin;

                            vicinity_tree[(vicinity_offset + v) * num_sub + sub] =
                                vt_child; // FIXME: No need to write all the data.
                        }
                    });
                    vicinity_tree.barrier();

                    totnum_particles = work_group_reduce_add(totnum_particles, local_size());

                    const gpu_uint num_own_particles =
                        local_tree[(group_id() * tree_depthbits * (1 << MAX_BIGNODE_BITS())
                                    + (depth_bm - 1) * (1 << MAX_BIGNODE_BITS()) + ((1 << MAX_BIGNODE_BITS()) - 1))
                                       * num_sub
                                   + num_sub - 1]
                            .pend
                        - local_tree[(group_id() * tree_depthbits * (1 << MAX_BIGNODE_BITS())
                                      + (depth_bm - 1) * (1 << MAX_BIGNODE_BITS()))
                                     * num_sub]
                              .pbegin;

                    const gpu_bool step_in = (gpu_float(totnum_particles) * num_own_particles
                                              > 2 * vdata.access_list.size() * num_sub * (1 << MAX_BIGNODE_BITS())
                                                    * num_sub * MULTIPOLE_COSTFAC());
                    const gpu_uint other_sub = local_id() / num_sub;

                    assert((1u << MAX_BIGNODE_BITS()) <= num_sub);
                    gpu_if(local_id() < num_sub * (1u << MAX_BIGNODE_BITS()))
                    {
                        gpu_uint child = other_sub;
                        gpu_uint childnum = child & 1;
                        if (MAX_BIGNODE_BITS() == 0)
                            childnum = gpu_uint(bignode_is_child1);
                        const gpu_uint parent_sub = (sub >> 1) | ((childnum) << (this->sub_bits - 1));
                        const Vector<gpu_uint, 3> pos = { child % (1 << bitvec[0]),
                                                          (child >> bitvec[0]) % (1 << bitvec[1]),
                                                          (child >> (bitvec[0] + bitvec[1])) };
                        const gpu_uint localpos =
                            pos[0] + pos[1] * vdata.sizevec[0] + pos[2] * vdata.sizevec[0] * vdata.sizevec[1];
                        Vector<gpu_uint, 3> parent_pos = { pos[2], pos[0] / 2, pos[1] };
                        if (MAX_BIGNODE_BITS() != 0)
                            for (Tuint k = 0; k < 3; ++k)
                            {
                                parent_pos[k] = cond(bignode_is_child1 && ((MAX_BIGNODE_BITS() + 1) % 3 == 2 - k),
                                                     parent_pos[k] + (1u << (bitvec[k] - 1)),
                                                     parent_pos[k]);
                            }
                        const gpu_uint parent_localpos = parent_pos[0] + parent_pos[1] * (1 << bitvec[0])
                                                         + parent_pos[2] * (1 << (bitvec[0] + bitvec[1]));
                        const gpu_uint lt_parent_p = group_id() * tree_depthbits * (1 << MAX_BIGNODE_BITS())
                                                     + (depth_bm - 1) * (1 << MAX_BIGNODE_BITS()) + parent_localpos;

                        const Vector<gpu_T, 3> center_child_r =
                            bignode_center_r + COORDS.getpos_r(pos.cast<gpu_int>()) + COORDS.getsubshift_r(sub);

                        coordinates D = COORDS;
                        for (Tint k = this->sub_bits - 1; k >= 0; --k)
                            D.move_down();

                        Vector<gpu_T, 3> shift_r = { 0, 0, 0 };
                        shift_r[this->sub_bits % 3] = D.shiftvec[0] * ((sub & 1) - 0.5f);

                        const gpu_uint lt_pb =
                            vicinity_tree[(vicinity_offset + localpos + local_offset) * num_sub + sub].pbegin;
                        const gpu_uint lt_pe =
                            vicinity_tree[(vicinity_offset + localpos + local_offset) * num_sub + sub].pend;

                        const gpu_uint lt_p = group_id() * tree_depthbits * (1 << MAX_BIGNODE_BITS())
                                              + depth_bm * (1 << MAX_BIGNODE_BITS()) + child;

                        const multipole<gpu_T, max_multipole> oldMr =
                            local_tree[lt_parent_p * num_sub + parent_sub].Mr.rot();
                        local_tree[lt_p * num_sub + sub].Mr = oldMr.shift_loc(shift_r);

                        const gpu_uint begin = max(min(lt_pb, pend), pbegin);
                        const gpu_uint end = max(min(lt_pe, pend), pbegin);
                        local_tree[lt_p * num_sub + sub].pbegin = begin;
                        local_tree[lt_p * num_sub + sub].pend = end;
                    }
                    local_tree.barrier();

                    gpu_for(0, (1u << MAX_BIGNODE_BITS()), [&](gpu_uint16 child) {
                        ++COUNT[5];

                        const Vector<gpu_uint, 3> pos = { child % (1 << bitvec[0]),
                                                          (child >> bitvec[0]) % (1 << bitvec[1]),
                                                          (child >> (bitvec[0] + bitvec[1])) };
                        const gpu_uint localpos =
                            pos[0] + pos[1] * vdata.sizevec[0] + pos[2] * vdata.sizevec[0] * vdata.sizevec[1];

                        const gpu_uint lt_p = group_id() * tree_depthbits * (1 << MAX_BIGNODE_BITS())
                                              + depth_bm * (1 << MAX_BIGNODE_BITS()) + child;

                        const Vector<gpu_T, 3> center_child_r =
                            bignode_center_r + COORDS.getpos_r(pos.cast<gpu_int>()) + COORDS.getsubshift_r(sub);

                        multipole<gpu_T, max_multipole> newMr =
                            multipole<T, max_multipole>::from_particle({ 0, 0, 0 }, 0);
                        gpu_if(other_sub == 0)
                        {
                            newMr = local_tree[lt_p * num_sub + sub].Mr;
                        }

                        if (true)
                        {
                            local_mem<multipole<T, max_multipole>> vicinity_cache(local_size());
                            gpu_for(0, vdata.update_list.size() / num_sub, [&](gpu_uint vubase) {
                                ++COUNT[6];
                                gpu_uint v = vicinity_update_list[vubase * num_sub + other_sub];
                                v = cond((child & 1) == 0, v, v + 2 * vdata.maxvec[0] - 2 * (v % vdata.sizevec[0]));

                                vicinity_cache[local_id()] =
                                    vicinity_tree[(vicinity_offset + v + localpos) * num_sub + sub].Mr;
                                vicinity_cache.barrier();

                                gpu_for(0, num_sub, [&](gpu_uint s) {
                                    gpu_uint vl = vicinity_update_list[vubase * num_sub + s];
                                    vl = cond(
                                        (child & 1) == 0, vl, vl + 2 * vdata.maxvec[0] - 2 * (vl % vdata.sizevec[0]));
                                    Vector<gpu_int, 3> vpos = { vl % vdata.sizevec[0],
                                                                (vl / vdata.sizevec[0]) % vdata.sizevec[1],
                                                                vl / vdata.sizevec[0] / vdata.sizevec[1] };

                                    Vector<gpu_int, 3> localpos =
                                        pos.cast<gpu_int>() + vpos - vdata.maxvec.template cast<gpu_int>();

                                    const Vector<gpu_T, 3> vicinity_center_r =
                                        bignode_center_r + COORDS.getpos_r(localpos) + COORDS.getsubshift_r(other_sub);

                                    newMr += vicinity_cache[s * num_sub + other_sub].makelocal(center_child_r
                                                                                               - vicinity_center_r);
                                });

                                vicinity_cache.barrier();
                                // FIXME: Is it possible to split shifting by dimension and combine calculation across
                                // multiple nodes?
                            });
                            if (vdata.update_list.size() % num_sub != 0)
                            {
                                const gpu_uint vubase = vdata.update_list.size() / num_sub;

                                gpu_uint v = vicinity_update_list[min(vubase * num_sub + other_sub,
                                                                      vdata.update_list.size() - 1)];
                                v = cond((child & 1) == 0, v, v + 2 * vdata.maxvec[0] - 2 * (v % vdata.sizevec[0]));

                                vicinity_cache[local_id()] =
                                    vicinity_tree[(vicinity_offset + v + localpos) * num_sub + sub].Mr;
                                vicinity_cache.barrier();

                                gpu_for(0, vdata.update_list.size() % num_sub, [&](gpu_uint s) {
                                    gpu_uint vl = vicinity_update_list[vubase * num_sub + s];
                                    vl = cond(
                                        (child & 1) == 0, vl, vl + 2 * vdata.maxvec[0] - 2 * (vl % vdata.sizevec[0]));
                                    Vector<gpu_int, 3> vpos = { vl % vdata.sizevec[0],
                                                                (vl / vdata.sizevec[0]) % vdata.sizevec[1],
                                                                vl / vdata.sizevec[0] / vdata.sizevec[1] };

                                    Vector<gpu_int, 3> localpos =
                                        pos.cast<gpu_int>() + vpos - vdata.maxvec.template cast<gpu_int>();

                                    const Vector<gpu_T, 3> vicinity_center_r =
                                        bignode_center_r + COORDS.getpos_r(localpos) + COORDS.getsubshift_r(other_sub);

                                    newMr += vicinity_cache[s * num_sub + other_sub].makelocal(center_child_r
                                                                                               - vicinity_center_r);
                                });

                                vicinity_cache.barrier();
                                // FIXME: Is it possible to split shifting by dimension and combine calculation across
                                // multiple nodes?
                            }
                        }

                        for (Tuint shift = local_size() / 2; shift >= num_sub; shift /= 2)
                        {
                            newMr += shuffle_xor(newMr, shift, shift * 2);
                        }

                        const gpu_uint pbegin = local_tree[lt_p * num_sub + sub].pbegin;
                        const gpu_uint pend = local_tree[lt_p * num_sub + sub].pend;
                        local_tree.barrier();
                        gpu_if(other_sub == 0)
                        {
                            local_tree[lt_p * num_sub + sub].Mr = newMr; // FIXME: Parallelize.
                        }
                        gpu_if(!step_in)
                        {
                            for (Tuint mod3 = 0; mod3 < 3; ++mod3)
                            {
                                gpu_if(child_mod3 == mod3)
                                {
                                    gpu_for(pbegin + other_sub, pend, num_sub, [&](gpu_uint p) {
                                        Vector<gpu_T, 3> F =
                                            rot(newMr.calc_force(rot(x[p], mod3) - center_child_r), -(Tint)mod3);
#if CALC_POTENTIAL
                                        const gpu_T pot_r = newMr.calc_loc_potential(rot(x[p], mod3) - center_child_r);
#endif
                                        v[p] += F * (gpu_T)DT();
#if CALC_POTENTIAL
                                        potential[p] = pot_r;
#endif
                                    });
                                }
                            }
                        }
                    });
                    local_tree.barrier();
                    v.barrier();
#if CALC_POTENTIAL
                    potential.barrier();
#endif

                    gpu_if(!step_in)
                    {
                        ++COUNT[8];

                        {
                            gpu_for(0, (1u << MAX_BIGNODE_BITS()), [&](gpu_uint n) {
                                const gpu_uint ln_p = group_id() * tree_depthbits * (1 << MAX_BIGNODE_BITS())
                                                      + depth_bm * (1 << MAX_BIGNODE_BITS()) + n;
                                gpu_uint begin = local_tree[ln_p * num_sub].pbegin;
                                gpu_uint end = local_tree[(ln_p + 1) * num_sub - 1].pend;
                                const Vector<gpu_uint, 3> pos = { n % (1 << bitvec[0]),
                                                                  (n >> bitvec[0]) % (1 << bitvec[1]),
                                                                  (n >> (bitvec[0] + bitvec[1])) };
                                const gpu_uint localpos =
                                    pos[0] + pos[1] * vdata.sizevec[0] + pos[2] * vdata.sizevec[0] * vdata.sizevec[1];

                                auto FUNC_CORE = [&](Tuint ID,
                                                     Tuint blocksize,
                                                     gpu_uint a,
                                                     gpu_uint tid,
                                                     Tuint tsize,
                                                     gpu_bool use) {
                                    (void)ID;
                                    //++COUNT[ID+0];
                                    vector<Vector<gpu_T, 3>> F(blocksize, { 0, 0, 0 });
                                    vector<gpu_T> P(blocksize, 0);
                                    gpu_if(use)
                                    {
                                        gpu_for(0, vdata.local_list.size(), [&](gpu_uint locu) {
                                            //++COUNT[ID+1];
                                            const gpu_uint loc = vicinity_local_list[locu] + localpos;

                                            gpu_for(vicinity_tree[(vicinity_offset + loc) * num_sub].pbegin + tid,
                                                    vicinity_tree[(vicinity_offset + loc) * num_sub + num_sub - 1].pend,
                                                    tsize,
                                                    [&](gpu_uint b) {
                                                        ++COUNT[ID + 0];
                                                        for (Tuint k = 0; k < blocksize; ++k)
                                                        {
                                                            const Vector<gpu_T, 3> dist = x[b] - x[a + k];
                                                            F[k] += dist
                                                                    * (mass[b] * pow<-1, 2>(dist.squaredNorm() + 1E-20f)
                                                                       * pow2(pow<-1, 2>(dist.squaredNorm() + 1E-20f)));
                                                            P[k] += cond(
                                                                b == a + k,
                                                                0.f,
                                                                -(mass[b] * pow<-1, 2>(dist.squaredNorm() + 1E-20f)));
                                                        }
                                                    });
                                        });
                                    }
                                    if (tsize > 1)
                                    {
                                        local_mem<Vector<T, 3>> LF(local_size() * blocksize);
                                        local_mem<T> LP(local_size() * blocksize);
                                        for (Tuint k = 0; k < blocksize; ++k)
                                        {
                                            LF[local_id() * blocksize + k] = F[k];
                                            LP[local_id() * blocksize + k] = P[k];
                                        }
                                        LF.barrier();
                                        LP.barrier();
                                        gpu_if(use)
                                        {
                                            gpu_for(tid, blocksize, tsize, [&](gpu_uint k) {
                                                Vector<gpu_T, 3> totF = { 0, 0, 0 };
                                                gpu_T totP = 0;
                                                for (Tuint t = 0; t < tsize; ++t)
                                                {
                                                    totF += LF[(t + local_id() - tid) * blocksize + k];
                                                    totP += LP[(t + local_id() - tid) * blocksize + k];
                                                }
                                                v[a + k] += totF * (gpu_T)DT();
#if CALC_POTENTIAL
                                                potential[a + k] += totP;
#endif
                                            });
                                        }
                                        LF.barrier(); // FIXME: Shouldn't be necessary.
                                        LP.barrier();
                                    }
                                    else
                                    {
                                        for (Tuint k = 0; k < blocksize; ++k)
                                        {
                                            v[a + k] += F[k] * (gpu_T)DT();
#if CALC_POTENTIAL
                                            potential[a + k] += P[k];
#endif
                                        }
                                    }
                                };

                                auto FUNC = [&](Tuint ID,
                                                Tuint blocksize,
                                                gpu_uint sid,
                                                Tuint ssize,
                                                gpu_uint tid,
                                                Tuint tsize) {
                                    gpu_while(end - begin >= ssize * blocksize)
                                    {
                                        const gpu_uint a = begin + sid * blocksize;
                                        FUNC_CORE(ID, blocksize, a, tid, tsize, true);
                                        begin += ssize * blocksize;
                                    }
                                };
                                FUNC(9, 5, local_id(), local_size(), 0, 1);
                                FUNC(10, 5, other_sub, num_sub, sub, num_sub);
                                FUNC(11, 1, other_sub, num_sub, sub, num_sub);
                                if (DEBUG1)
                                    gpu_assert(end - begin < num_sub);
                                gpu_uint a = begin + other_sub;
                                FUNC_CORE(12, 1, a, sub, num_sub, a < end);
                            });
                        }

                        gpu_while((id_bignode & 1) != 0)
                        {
                            COORDS.move_up();
                            --depth_bm;
                            child_mod3 = cond(child_mod3 == 0, 2u, child_mod3 - 1);
                            bignodeshift_and = cond(child_mod3 == (this->sub_bits + MAX_BIGNODE_BITS() + 1) % 3,
                                                    (bignodeshift_and >> 1)
                                                        | (bignodeshift_and_t(0x80000000)
                                                           << (get_size<bignodeshift_and_t>::value == 8 ? 32 : 0)),
                                                    bignodeshift_and);
                            id_bignode >>= 1;
                            {
                                coordinates E = COORDS;
                                for (Tuint k = 0; k < MAX_BIGNODE_BITS() - 1; ++k)
                                    E.move_up();
                                bignode_center_r[2 - (MAX_BIGNODE_BITS() + 2) % 3] -=
                                    reinterpret<gpu_T>(reinterpret<bignodeshift_and_t>(E.shiftvec[0])
                                                       & bignodeshift_and)
                                    / 2;
                                bignode_center_r = rot(bignode_center_r, -1);
                            }
                        }
                        id_bignode |= 1;
                        {
                            coordinates E = COORDS;
                            for (Tuint k = 0; k < MAX_BIGNODE_BITS(); ++k)
                                E.move_up();
                            bignode_center_r[2 - (MAX_BIGNODE_BITS() + 2) % 3] +=
                                reinterpret<gpu_T>(reinterpret<bignodeshift_and_t>(E.shiftvec[0]) & bignodeshift_and);
                        }
                    }
                    gpu_else
                    {
                        {
                            coordinates E = COORDS;
                            for (Tuint k = 0; k < MAX_BIGNODE_BITS() - 1; ++k)
                                E.move_up();
                            bignode_center_r = rot(bignode_center_r);
                            bignode_center_r[2 - (MAX_BIGNODE_BITS() + 2) % 3] -=
                                reinterpret<gpu_T>(reinterpret<bignodeshift_and_t>(E.shiftvec[0]) & bignodeshift_and)
                                / 2;
                        }
                        COORDS.move_down();
                        ++depth_bm;
                        bignodeshift_and = cond(child_mod3 == (this->sub_bits + MAX_BIGNODE_BITS() + 1) % 3,
                                                bignodeshift_and << 1,
                                                bignodeshift_and);
                        child_mod3 = cond(child_mod3 == 2, 0u, child_mod3 + 1);
                        id_bignode <<= 1;

                        gpu_assert(depth_bm < tree_depthbits);
                    }
                }
            },
            this->ls_use);

        local_tree.assign(device,
                          downwards.num_groups() * tree_depthbits * (1 << MAX_BIGNODE_BITS())
                              * (1u << log2_exact(this->ls_use) / 2));
        vicinity_tree.assign(
            device, downwards.num_groups() * tree_depthbits * vdata.size * (1u << log2_exact(this->ls_use) / 2));

        local_tree.fill(
            { .pbegin = 0, .pend = numeric_limits<typename multipole<T, max_multipole>::uint_type>::max(), .Mr = {} });

        vicinity_tree.fill({ .Mr = {}, .first_child = 0, .pbegin = 0, .pend = 0 });
    }
};

int main(int argc, char** argv)
{
    init_params(argc, argv);

    unique_ptr<sdl_window> window = sdl_window::create("fmm nbody",
                                                       { 1024, 768 },
                                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                                       static_cast<goopax::envmode>(env_ALL & ~env_VULKAN));
    goopax_device device = window->device;

#if GOOPAX_DEBUG
    // Increasing number of threads to be able to check for race conditions.
    device.force_global_size(192);
#endif

#if WITH_METAL
    particle_renderer Renderer(dynamic_cast<sdl_window_metal&>(*window));
    buffer<Vector3<Tfloat>> x(device, NUM_PARTICLES()); // OpenGL buffer
    buffer<Vector4<Tfloat>> color(device, NUM_PARTICLES());
#elif WITH_OPENGL
    opengl_buffer<Vector3<Tfloat>> x(device, NUM_PARTICLES()); // OpenGL buffer
    opengl_buffer<Vector4<Tfloat>> color(device, NUM_PARTICLES());
#else
    buffer<Vector3<Tfloat>> x(device, NUM_PARTICLES());
    buffer<Vector4<Tfloat>> color(device, NUM_PARTICLES());
#endif

    cosmos<Tfloat, MULTIPOLE_ORDER> Cosmos(device, NUM_PARTICLES(), MAX_DISTFAC());

    if (argc >= 2)
    {
        Cosmos.make_IC(argv[1]);
    }

    if (PRECISION_TEST())
    {
        Cosmos.precision_test();
        return 0;
    }

    kernel set_colors(device, [&](const resource<Vector<Tfloat, 3>>& cx) {
        gpu_for_global(0, x.size(), [&](gpu_uint k) {
            color[k] = ::color(Cosmos.potential[k]);
            x[k] = cx[k];
            // Tweaking z coordinate to use potential for depth testing.
            // Particles are displayed according to their x and y coordinates.
            // If multiple particles are drawn at the same pixel, the one with the
            // highest potential will be shown.
            x[k][2] = -Cosmos.potential[k] * 0.01f;
        });
    });

    bool quit = false;
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
                    case SDLK_F:
                        window->toggle_fullscreen();
                        break;
                };
            }
        }

        static auto frametime = steady_clock::now();
        static Tint framecount = 0;

        Cosmos.step();

        auto now = steady_clock::now();
        ++framecount;
        if (now - frametime > chrono::seconds(1))
        {
            stringstream title;
            Tdouble rate = framecount / chrono::duration<double>(now - frametime).count();
            title << "N-body. N=" << x.size() << ", " << rate << " fps, device=" << device.name();
            string s = title.str();
            SDL_SetWindowTitle(window->window, s.c_str());
            framecount = 0;
            frametime = now;
        }

        set_colors(Cosmos.x);

#if WITH_METAL
        Renderer.render(x);
#elif WITH_OPENGL
        render(window->window, x, &color);
        SDL_GL_SwapWindow(window->window);
#else
        cout << "x=" << x << endl;
#endif
    }
    return 0;
}

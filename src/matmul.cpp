#include <random>

#include "common/draw/types.h"
#include <goopax_extra/param.hpp>
using namespace Eigen;

#include <cassert>
#include <goopax>
#if defined(GOOPAX_USE_BOOST) && GOOPAX_USE_BOOST
#include <boost/chrono.hpp>
using namespace boost::chrono;
#else
#include <chrono>
using namespace std::chrono;
#endif
using namespace goopax;
using namespace goopax::release;
using namespace std;
using namespace types;

PARAMOPT<Tsize_t> NK("nk", 2048); // Matrix sizes. Can be specified as command line arguments,
PARAMOPT<Tsize_t> NL("nl", 2048); // e.g., --nk=128 --nl=256 --nm=384
PARAMOPT<Tsize_t> NM("nm", 2048);
PARAMOPT<Tbool> VERIFY("verify", false); // Use --verify=1 to test the result.
PARAMOPT<Tbool> USE_DOUBLE("double", false);
PARAMOPT<Tint> USE_DEVICE("use_device", -1);

#ifdef _MSC_VER
#define vector std::vector
#endif

// GOOPAX currently does not provide 2-dimensional resource access. Implementing it here.

// The class matrix_wrapper provides matrix access to an arbitrary container class that has random-access iterators.
template<class RES>
struct matrix_wrapper
{
    RES res;
    using TYPE = typename std::remove_reference<RES>::type;
    using U = typename TYPE::size_type;

    const U height;
    const U width;

    typename TYPE::iterator operator[](const U& col)
    {
        return res.begin() + col * width;
    }

    typename TYPE::const_iterator operator[](const U& col) const
    {
        return res.begin() + col * width;
    }

    template<class... ARGS>
    matrix_wrapper(const U& height0, const U& width0, ARGS... args)
        : res(args...)
        , // Any additional arguments are passed to the constructor of RES.
        height(height0)
        , width(width0)
    {
    }

    matrix_wrapper(const U& height0, const U& width0, RES res0)
        : res(res0)
        , // Any additional arguments are passed to the constructor of RES.
        height(height0)
        , width(width0)
    {
    }
};

template<typename RES>
ostream& operator<<(ostream& s, const matrix_wrapper<RES>& m)
{
    for (Tsize_t y = 0; y < m.height; ++y)
    {
        for (Tsize_t x = 0; x < m.width; ++x)
        {
            if (x != 0)
                s << " ";
            s << m.res[y * m.width + x];
        }
        s << endl;
    }
    return s;
}

// Naive implementation of a matrix multiplication.
template<class float_type>
void matmul_naive(matrix_wrapper<resource<float_type>&>& C,
                  const matrix_wrapper<const resource<float_type>&>& A,
                  const matrix_wrapper<const resource<float_type>&>& B)
{
    using gpu_float_type = typename make_gpu<float_type>::type;

    gpu_for_global(0,
                   C.height * C.width,
                   [&](gpu_uint pos) // Parallel loop over all matrix elements
                   {
                       const gpu_uint k = pos / C.width;
                       const gpu_uint m = pos % C.width;

                       gpu_float_type Ctmp = 0;
                       gpu_for(0, A.width, [&](gpu_uint l) { Ctmp += A[k][l] * B[l][m]; });
                       C[k][m] = Ctmp;
                   });
}

// More sophisticated matrix multiplication. Uses registers to cache matrix elements.
// Good performance on AMD cards, poor performance on nvidia cards.
template<class float_type>
void matmul_reg(matrix_wrapper<resource<float_type>&>& C,
                const matrix_wrapper<const resource<float_type>&>& A,
                const matrix_wrapper<const resource<float_type>&>& B)
{
    using gpu_float_type = typename make_gpu<float_type>::type;

    // Block sizes. Using fine-tuned values here for simplicity. If not enough registers are available, these values
    // need to be reduced.
    Tuint B1k = 8;
    Tuint B1l = 2;
    Tuint B1m = 8;
    if (sizeof(float_type) == 8) // Adjustments for Tdouble precision.
    {
        B1k = 8;
        B1l = 1;
    }

    // Big parallel loop over all blocks in the result matrix C.
    gpu_for_group(0, C.height, B1k, [&](gpu_uint koffset) {
        gpu_for_local(0, C.width, B1m, [&](gpu_uint moffset) {
            // Caching the result block. Storing intermediate results in Ctmp as registers.
            Eigen::Matrix<gpu_float_type, Dynamic, Dynamic> Ctmp(B1k, B1m);
            Ctmp.fill(0);

            gpu_for(0, A.width, B1l, [&](gpu_uint loffset) {
                // Calculating the matrix product of the sub-blocks.
                // There is no need to prefetch the sub-blocks of A and B into registers:
                // The c-style for-loops are explicitly unrolled,
                // and redundant memory accesses are removed by common subexpression elimination.
                for (unsigned int ksub = 0; ksub < B1k; ++ksub)
                {
                    const gpu_uint k = koffset + ksub;
                    for (unsigned int msub = 0; msub < B1m; ++msub)
                    {
                        const gpu_uint m = moffset + msub;
                        for (Tuint lsub = 0; lsub < B1l; ++lsub)
                        {
                            const gpu_uint l = loffset + lsub;
                            Ctmp(ksub, msub) += A[k][l] * B[l][m];
                        }
                    }
                }
            });

            // Writing the result block.
            for (Tuint ksub = 0; ksub < B1k; ++ksub)
            {
                const gpu_uint k = koffset + ksub;
                for (Tuint msub = 0; msub < B1m; ++msub)
                {
                    const gpu_uint m = moffset + msub;
                    C[k][m] = Ctmp(ksub, msub);
                }
            }
        });
    });
}

// The following implementation uses two-level caching: Big blocks in local memory for work-groups, and small blocks in
// registers for individual threads. Good performance on nvidia cards, not so good performance on AMD cards.
template<class float_type>
void matmul_reg_and_localmem(matrix_wrapper<resource<float_type>&>& C,
                             const matrix_wrapper<const resource<float_type>&>& A,
                             const matrix_wrapper<const resource<float_type>&>& B)
{
    using gpu_float_type = typename make_gpu<float_type>::type;

    // Sizes of small blocks for register cache.
    Tuint B1k = 8;
    Tuint B1l = 2;
    Tuint B1m = 8;
    if (sizeof(float_type) == 8)
    {
        B1k = 8;
        B1l = 1;
    }

    // Sizes of big blocks for local memory cache. Assigning one block to every work-group.
    Tuint B2k = 1;
    while (B2k * B2k * 4 <= local_size())
    {
        B2k *= 2;
    }
    Tuint B2m = B2k;
    if (B2k * B2m < local_size())
        B2m *= 2;

    Tuint B2l = 16;
    if (sizeof(float_type) == 8)
        B2l = 8;
    assert(B2k * B2m == local_size());

    // Total big block sizes.
    const Tuint B12k = B1k * B2k;
    const Tuint B12l = B1l * B2l;
    const Tuint B12m = B1m * B2m;

    // Number of big blocks
    gpu_uint blocks_k = C.height / B12k;
    gpu_uint blocks_l = A.width / B12l;
    gpu_uint blocks_m = C.width / B12m;

    // Loop through all big blocks, parallelized over the work-groups.
    gpu_for_group(0, blocks_k * blocks_m, [&](gpu_uint pos3) {
        // The work-group handles big block k3,m3
        const gpu_uint k3 = pos3 / blocks_m;
        const gpu_uint m3 = pos3 % blocks_m;

        // The individual thread handles small block k2,m2 within the big block.
        const gpu_uint k2 = local_id() / B2m;
        const gpu_uint m2 = local_id() % B2m;

        // Caching the result block
        vector<vector<gpu_float_type>> Ctmp(B1k, vector<gpu_float_type>(B1m, 0));

        gpu_for(0, blocks_l, [&](gpu_uint l3) {
            // Allocating local memory.
            matrix_wrapper<local_mem<float_type>> Atmp(B12k, B12l, B12k * B12l);
            matrix_wrapper<local_mem<float_type>> Btmp(B12l, B12m, B12l * B12m);
            Tuint burstl = 2;
            Tuint burstm = 4;

            // Fetch memory into local memory.
            gpu_for_local(0, B12k * B12l, burstl, [&](gpu_uint pos12) {
                const gpu_uint k = pos12 / B12l;
                const gpu_uint l_start = pos12 % B12l;

                for (Tuint sub = 0; sub < burstl; ++sub)
                {
                    const gpu_uint l = l_start + sub;
                    Atmp[k][l] = A[k3 * B12k + k][l3 * B12l + l];
                }
            });
            gpu_for_local(0, B12l * B12m, burstm, [&](gpu_uint pos12) {
                const gpu_uint l = pos12 / B12m;
                const gpu_uint m_start = pos12 % B12m;

                for (Tuint sub = 0; sub < burstm; ++sub)
                {
                    const gpu_uint m = m_start + sub;
                    Btmp[l][m] = B[l3 * B12l + l][m3 * B12m + m];
                }
            });
            Atmp.res.barrier();
            Btmp.res.barrier();

            // Now calculate the contribution to sub-block k2,m2. Again, common subexpression elimination will remove
            // Redundand memory accesses, and most of the actual calculation is done in registers.
            gpu_for(0, B2l, [&](gpu_uint l2) {
                for (Tuint k = 0; k < B1k; ++k)
                {
                    for (Tuint m = 0; m < B1m; ++m)
                    {
                        for (Tuint l = 0; l < B1l; ++l)
                        {
                            Ctmp[k][m] += Atmp[k2 * B1k + k][l2 * B1l + l] * Btmp[l2 * B1l + l][m2 * B1m + m];
                        }
                    }
                }
            });
        });

        // Write the result.
        for (Tuint k = 0; k < B1k; ++k)
        {
            for (Tuint m = 0; m < B1m; ++m)
            {
                C[k3 * B12k + k2 * B1k + k][m3 * B12m + m2 * B1m + m] = Ctmp[k][m];
            }
        }
    });
}

// This is the kernel class.
template<class float_type>
struct matmul
{
    decltype(matmul_naive<float_type>)& matmulfunc; // function pointer

    kernel<void(buffer<float_type>& Craw,
                const buffer<float_type>& Araw,
                const buffer<float_type>& Braw,
                Tuint Nk,
                Tuint Nl,
                Tuint Nm)>
        Kernel;

    void run(goopax_device device);

    matmul(goopax_device device, decltype(matmulfunc)& matmulfunc0)
        : matmulfunc(matmulfunc0)
    {
        Kernel.assign(device,
                      [this](resource<float_type>& Craw,
                             const resource<float_type>& Araw,
                             const resource<float_type>& Braw,
                             gpu_uint Nk,
                             gpu_uint Nl,
                             gpu_uint Nm) {
                          // Declare matrix resources.
                          matrix_wrapper<resource<float_type>&> C(Nk, Nm, Craw);
                          const matrix_wrapper<const resource<float_type>&> A(Nk, Nl, Araw);
                          const matrix_wrapper<const resource<float_type>&> B(Nl, Nm, Braw);

                          // Call the multiplication. The actual algorithm is outsources into matmulfunc.
                          matmulfunc(C, A, B);
                      });
    }
};

template<class float_type>
void matmul<float_type>::run(goopax_device device)
{
    // This function does the test setup for the specified precision and algorithm.

    // Allocate memory on video card.
    buffer<float_type> A(device, NK() * NL());
    buffer<float_type> B(device, NL() * NM());
    buffer<float_type> C(device, NK() * NM());
    C.fill(-1);

    // Allocate cpu memory for comparison.
    matrix_wrapper<vector<float_type>> Acpu(NK(), NL(), NK() * NL());
    matrix_wrapper<vector<float_type>> Bcpu(NL(), NM(), NL() * NM());
    matrix_wrapper<vector<float_type>> Ccpu(NK(), NM(), NK() * NM());

    std::default_random_engine generator;
    std::normal_distribution<float_type> distribution;
    // Fill A and B matrices with some values.
    for (Tuint k = 0; k < NK(); ++k)
    {
        for (Tuint l = 0; l < NL(); ++l)
        {
            Acpu[k][l] = distribution(generator);
        }
    }
    for (Tuint l = 0; l < NL(); ++l)
    {
        for (Tuint m = 0; m < NM(); ++m)
        {
            Bcpu[l][m] = distribution(generator);
        }
    }

    A.copy_from_host(Acpu.res.data());
    B.copy_from_host(Bcpu.res.data());

    // Instantiate the kernel

    // Call the kernel. This will calculate C=A*B. The first call may take longer due to initialization.
    Kernel(C, A, B, NK(), NL(), NM());

    C.copy_to_host(Ccpu.res.data());

    // Display matrices if they are reasonably small.
    if (NK() * NL() + NL() * NM() < 10000)
    {
        cout << "\nA=\n"
             << Acpu << "\n"
             << "\nB=\n"
             << Bcpu << "\n"
             << "\nC=\n"
             << Ccpu << "\n"
             << endl;
    }

    // Now do performance measurements.
    for (Tuint k = 0; k < 3; ++k)
    {
        Tsize_t count = 0;
        auto time_start = high_resolution_clock::now();
        while (high_resolution_clock::now() < time_start + seconds(1))
        {
            Kernel(C, A, B, NK(), NL(), NM());
            ++count;
            device.wait_all();
        }
        Tdouble time = duration_cast<duration<double>>(high_resolution_clock::now() - time_start).count();
        auto FLOPS = Tdouble(NK()) * NL() * NM() * 2 * count / time;
        cout << "Did " << count << " matrix multiplications in " << time << " seconds. Performance: " << FLOPS / 1E12
             << " TFLOPS" << endl;
    }

    // Verify the result if --verify=1 has been specified
    if (VERIFY())
    {
        float_type maxerr = 0;
        for (Tsize_t k = 0; k < NK(); ++k)
        {
            cout << "k=" << k << " / " << NK() << endl;
            for (Tsize_t m = 0; m < NM(); ++m)
            {
                float_type Ctmp = 0;
                for (Tuint l = 0; l < NL(); ++l)
                {
                    Ctmp += Acpu[k][l] * Bcpu[l][m];
                }
                maxerr = max(maxerr, abs(Ctmp - Ccpu[k][m]));
                if (!(abs(Ctmp - Ccpu[k][m]) < 1E-3))
                {
                    std::cerr << "Results differ: cpu: C[" << k << "][" << m << "]=" << Ctmp << ", gpu: C[" << k << "]["
                              << m << "]=" << Ccpu[k][m] << endl;
                    throw EX::general();
                }
            }
        }
        cout << "verification ok." << endl;
        cout << "maxerr=" << maxerr << endl;
    }
}

template<typename float_type>
void run(goopax_device device)
{
    matmul<float_type> naive(device, matmul_naive<float_type>);
    matmul<float_type> reg(device, matmul_reg<float_type>);
    matmul<float_type> reg_lm(device, matmul_reg_and_localmem<float_type>);

    {
        cout << "\nnaive:" << endl;
        naive.run(device);

        cout << "\nreg:" << endl;
        reg.run(device);

        cout << "\nreg and localmem:" << endl;
        reg_lm.run(device);
    }
}

int main(int argc, char** argv)
{
    init_params(argc, argv);

    goopax_device device = default_device();

    if (USE_DEVICE() != -1)
    {
        device = devices()[USE_DEVICE()];
    }

    assert(NK() % 128 == 0); // For simplicity, require matrix sizes to be multiples of 128.
    assert(NL() % 128 == 0);
    assert(NM() % 128 == 0);

    if (USE_DOUBLE())
        run<Tdouble>(device);
    else
        run<Tfloat>(device);
}

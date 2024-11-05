// @@@ CONVERT_TYPES_IGNORE @@@

#include <cassert>
#include <chrono>
#include <eigen3/Eigen/Eigen>
#include <goopax.h>
#include <random>
using namespace std::chrono;
using namespace goopax;
using namespace goopax::release;
using namespace std;
using namespace Eigen;

struct matmul_simple : public kernel<matmul_simple>
{
    void program(
        resource<float>& C, const resource<float>& A, const resource<float>& B, gpu_uint NK, gpu_uint NL, gpu_uint NM)
    {
        gpu_for_group(gpu_uint k = 0, NK) // row loop, parallelized over work-groups
        {
            gpu_for_local(gpu_uint m = 0, NM) // column loop, parallelized over local threads in work-group
            {
                gpu_float Ctmp = 0;

                gpu_for(gpu_uint l = 0, NL) // Loop over all blocks in matrices A and B that contribute to result block.
                {
                    Ctmp += A[k * NL + l] * B[l * NM + m];
                }
                C[k * NM + m] = Ctmp;
            }
        }
    }
};

struct matmul_blocked : public kernel<matmul_blocked>
{
    void program(
        resource<float>& C, const resource<float>& A, const resource<float>& B, gpu_uint NK, gpu_uint NL, gpu_uint NM)
    {
        const int Bk = 4;
        const int Bl = 4;
        const int Bm = 4;

        gpu_for_group(gpu_uint k_off = 0, NK, Bk) // row loop with step size Bk, parallelized over work-groups
        {
            gpu_for_local(gpu_uint m_off = 0,
                          NM,
                          Bm) // column loop with step size Bm, parallelized over local threads in work-group
            {
                // We will now calculate the block of the result matrix ranging from rows [k_off] to [k_off + Bk-1] and
                // columns [m_off] to [m_off + Bm-1].
                Matrix<gpu_float, Dynamic, Dynamic> Ctmp(
                    Bk, Bm);    // Allocating registers to hold result matrix block of size Bk*Bm,
                Ctmp.setZero(); // initialized with 0

                gpu_for(gpu_uint l_off = 0,
                        NL,
                        Bl) // Loop over all blocks in matrices A and B that contribute to result block.
                {
                    Matrix<gpu_float, Dynamic, Dynamic> Atmp(
                        Bk, Bl); // Allocating temporary block of matrices A and B in registers
                    Matrix<gpu_float, Dynamic, Dynamic> Btmp(Bl, Bm);

                    // Reading data from matrices A and B. We don't need to care about the ordering, GOOPAX will
                    // optimize and vectorize the memory access.
                    for (int l = 0; l < Bl; ++l)
                    {
                        for (int k = 0; k < Bk; ++k)
                            Atmp(k, l) = A[(k_off + k) * NL + l_off + l];
                        for (int m = 0; m < Bm; ++m)
                            Btmp(l, m) = B[(l_off + l) * NM + m_off + m];
                    }

                    // Doing the actual matrix multiplication of sub-blocks Atmp and Btmp, adding result to Ctmp
                    Ctmp += Atmp * Btmp;
                }

                // Writing the result block.
                for (int k = 0; k < Bk; ++k)
                    for (int m = 0; m < Bm; ++m)
                        C[(k_off + k) * NM + m_off + m] = Ctmp(k, m);
            }
        }
    }
};

Int main(Int argc, char** argv)
{
    goopax_env Env(argc, argv); // Initialize goopax
    goopax_device* device = Env.default_device();
#if USE_THREAD_OPTIMIZATIONS
    Env.thread_optimizations(device, true);
#endif

    const int NK = 1024;
    const int NL = 1024;
    const int NM = 1024;

    // Allocate memory on video card.
    buffer<float> A(device, NK * NL);
    buffer<float> B(device, NL * NM);
    buffer<float> C(device, NK * NM);

    // Filling A and B with random values.
    std::default_random_engine generator;
    std::normal_distribution<float> distribution;
    for (auto& p : A)
    {
        p = distribution(generator);
    }
    for (auto& p : B)
    {
        p = distribution(generator);
    }

    matmul_blocked Matmul;

    // Now do performance measurements.
    for (int k = 0; k < 3; ++k)
    {
        auto time_start = high_resolution_clock::now();
        Matmul(C, A, B, NK, NL, NM);
        Matmul.wait(); // Kernel calls are implicitly asynchronous. Need to wait to get correct time measurements.

        double time = duration<double>(high_resolution_clock::now() - time_start).count();
        auto FLOPS = double(NK) * NL * NM * 2 / time;
        cout << "time=" << time << " seconds. Performance: " << FLOPS / 1E12 << " TFLOPS" << endl;
    }

    cout << "Verifying result." << endl;
    for (int k = 0; k < NK; ++k)
    {
        for (int m = 0; m < NM; ++m)
        {
            double Ctmp = 0;
            for (int l = 0; l < NL; ++l)
            {
                Ctmp += A[k * NL + l] * B[l * NM + m];
            }
            if (!(abs(Ctmp - C[k * NM + m]) < 1E-3))
            {
                std::cerr << "Results differ: cpu: C[" << k << "][" << m << "]=" << Ctmp << ", gpu: C[" << k << "]["
                          << m << "]=" << C[k * NM + m] << endl;
                throw EX::general();
            }
        }
    }
    cout << "verification ok." << endl;
}

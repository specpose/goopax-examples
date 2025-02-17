/**
   \example gather.cpp
   Demonstrates the use of the gather mechanism to get return values from kernel calls.
 */

#include "common/draw/types.h"
#include <goopax>
#include <goopax_extra/output.hpp>
#include <goopax_extra/random.hpp>
#include <goopax_extra/struct_types.hpp>
#include <random>
using namespace goopax;
using namespace std;

// This kernel will gather various values of resource A: sum, min, max, bitwise or.
Tint main()
{
    goopax_device device = default_device(env_ALL);

    std::random_device rd;
    WELL512_data rnd(device, device.default_global_size_max(), rd());

    // This function will throw 1 million darts at random at buffer A and add the results.
    kernel ThrowDart(device, [&rnd](resource<Tuint>& A) {
        WELL512_lib rndlib(rnd);
        gpu_for_global(
            0, 1000000, [&](gpu_uint) { atomic_add(A[rndlib.generate()[0] % A.size()], 1, memory_order_relaxed); });
    });

    buffer<Tuint> A(device, 1024);
    A.fill(0);
    cout << "Throwing darts." << endl;
    ThrowDart(A);
    cout << "Got result:" << endl << A << endl;
    cout << "A.sum=" << A.sum() << endl;

    kernel Gatherkernel(device,
                        [](const resource<Tuint>& A,
                           gather_add<Tuint>& gsum,
                           gather_min<Tuint>& gmin,
                           gather_max<Tuint>& gmax) -> gather<Tuint, std::bit_or<>> {
                            gmin = numeric_limits<Tuint>::max();
                            gmax = 0;
                            gsum = 0;

                            gpu_uint ret = 0;
                            gpu_for_global(0, A.size(), [&](gpu_uint k) {
                                gsum += A[k];
                                gmin = min(gmin, A[k]);
                                gmax = max(gmax, A[k]);
                                ret |= A[k];
                            });
                            return ret;
                        });

    cout << "Calling gatherfunc." << endl;
    goopax_future<Tuint> min;
    goopax_future<Tuint> max;
    goopax_future<Tuint> sum;
    Tuint bor = Gatherkernel(A, sum, min, max).get();
    cout << "min=" << min.get() << endl
         << "max=" << max.get() << endl
         << "or=" << bor << endl
         << "sum=" << sum.get() << endl;
}

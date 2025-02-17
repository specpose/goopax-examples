/**
   \example pi.cpp
   Throwing darts to approximate the value of pi
 */

#include <chrono>
#include <goopax>
#include <goopax_extra/random.hpp>
#include <goopax_extra/struct_types.hpp>
#include <random>

using namespace goopax;
using namespace std;

static constexpr uint32_t N = 100000;

int main()
{
    vector<pair<unsigned int, std::function<goopax_future<uint64_t>()>>> kernels;

    std::random_device rd;
    for (goopax_device device : goopax::devices(env_ALL))
    {
        unsigned int Nsub;
        WELL512_data rnd(device, device.default_global_size_max(), rd());

        kernel newkernel(device, [&Nsub, &rnd]() -> gather_add<uint64_t> {
            WELL512_lib rndlib(rnd);

            gpu_uint num = 0;

            gpu_for(0, N, [&](gpu_int) {
                array<gpu_uint, 16> rnd_values = rndlib.generate();
                Nsub = rnd_values.size() / 2;
                for (unsigned int sub = 0; sub < Nsub; ++sub)
                {
                    // get x and y values in range 0..1
                    gpu_float x = rnd_values[2 * sub] * gpu_float(1.f / (uint64_t(1) << 32));
                    gpu_float y = rnd_values[2 * sub + 1] * gpu_float(1.f / (uint64_t(1) << 32));

                    num += (gpu_uint)(x * x + y * y < 1.f);
                }
            });
            return static_cast<gpu_uint64>(num);
        });
        cout << "Device " << kernels.size() << ": " << device.name() << ", #threads: " << newkernel.global_size()
             << ", envmode=" << device.get_envmode() << endl;

        kernels.emplace_back(Nsub, newkernel);
    }
    cout << endl;

    for (unsigned int k = 0; k < 10; ++k)
    {
        cout << "Running..." << std::flush;
        auto time_start = chrono::steady_clock::now();
        vector<pair<uint64_t, goopax_future<uint64_t>>> results;
        for (auto& k : kernels)
        {
            goopax_future<uint64_t> r = k.second();
            r.set_callback([i = int(&k - &kernels[0])]() { cout << i << std::flush; });
            results.push_back({ k.first, std::move(r) });
        }

        uint64_t darts = 0;
        uint64_t hits = 0;
        for (auto& r : results)
        {
            darts += uint64_t(N) * r.first * r.second.global_size();
            hits += r.second.get();
        }
        double pi = (4 * static_cast<double>(hits)) / darts;
        double time = chrono::duration<double>(chrono::steady_clock::now() - time_start).count();
        cout << " hit " << hits << "/" << darts << " darts -> pi=" << pi << ", time=" << time << " seconds." << endl;
    }
}

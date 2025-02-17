/**
   \example svm-pingpong.cpp
   Multiple devices exchange messages during the runtime of the kernel.
 */

#include <atomic>
#include <chrono>
#include <goopax>
#include <goopax_extra/param.hpp>
#include <thread>

using namespace goopax;
using namespace std::chrono;
using namespace std;

PARAMOPT<bool> WITH_CPU("with_cpu", false);
PARAMOPT<bool> FORCE("force", false);
PARAMOPT<unsigned int> ADD("add", 1);

template<typename T>
void pingpong()
{
    using gpu_T = typename make_gpu<T>::type;

    std::vector<kernel<void(T*, T*, unsigned int, T)>> kernels;

    auto devices = goopax::devices(env_ALL);

    unsigned int next_id = 0;
    unsigned int max_local_size = 0;
    for (unsigned int k = 0; k < devices.size(); ++k)
    {
        cout << "device " << k << ": " << devices[k].name() << ", support_svm()=" << devices[k].support_svm()
             << ", svm atomics: " << devices[k].support_svm_atomics();
        if (!devices[k].support_svm())
        {
            cout << "  ignoring device." << endl;
            devices.erase(devices.begin() + k);
            --k;
            continue;
        }
        if (!devices[k].support_svm_atomics() && !FORCE())
        {
            cout << "  ignoring device. Use anyway with --force=1" << endl;
            devices.erase(devices.begin() + k);
            --k;
            continue;
        }

        kernels.emplace_back(
            devices[k],
            [next_id](gpu_type<T*> ptr, gpu_type<T*> data, gpu_uint number_of_ids, gpu_T N) {
                const gpu_uint my_id = next_id + group_id();

                gpu_for(my_id, N, number_of_ids, [&](gpu_T expect) {
                    gpu_if(local_id() == 0)
                    {
                        //  Waiting for our turn.
                        gpu_while(atomic_load(*ptr, memory_order_acquire, memory::system) != expect)
                        {
                        }
                    }
                    local_barrier();

                    // Now it's our turn. Do some work.
                    data[local_id()] += 1;

                    local_barrier();

                    gpu_if(local_id() == 0)
                    {
                        // Handling over to the next id.
                        auto old = atomic_add(*ptr, ADD(), memory_order_release, memory::system);
                        gpu_assert(old == expect);
                    }
                });
            },
            0,
            (devices[k].get_envmode() == env_CPU ? devices[k].default_local_size() * 4 : 0));

        next_id += kernels.back().num_groups();
        max_local_size = max(max_local_size, kernels.back().local_size());
        cout << ", num_groups: " << kernels.back().num_groups() << endl;
    }
    if (devices.size() == 0)
    {
        cout << "No suitable device found. Leaving." << endl;
        return;
    }
    if (devices.size() == 1 && !WITH_CPU())
    {
        cout << "Only one device present. This is rather trivial and should succeed." << endl;
    }
    cout << endl;

    svm_buffer<T> svmbuf(devices[0], 1);
    svm_buffer<T> data(devices[0], max_local_size);
    std::fill(data.begin(), data.end(), 0);

    for (unsigned int N = 1; N != 0; N <<= 1)
    {
        svmbuf[0] = 0;

        auto t0 = steady_clock::now();

        for (auto& k : kernels)
        {
            k(svmbuf.data(), data.data(), next_id + WITH_CPU(), N);
        }

        if (WITH_CPU())
        {
#if defined(__cpp_lib_atomic_ref) && __cpp_lib_atomic_ref >= 201806
            atomic_ref<T> ref(svmbuf[0]);
            for (unsigned int expect = next_id; expect < N; expect += next_id + 1)
            {
                //  Waiting for our turn.
                while (ref.load(memory_order_acquire) != expect)
                {
                }

                // Now it's our turn. Do some work.
                data[0] += 1;

                // Handling over to the next id.
                ref.fetch_add(1, memory_order_release);
            }
#endif
        }

        wait_all_devices();
        auto dt = duration<double>(steady_clock::now() - t0);
        cout << "N=" << N << ", time=" << dt.count() << ", average pingpong time: " << dt.count() * 1E6 / N
             << " microseconds" << endl;
        if (dt > 1000ms)
            break;
    }
}

int main(int argc, char** argv)
{
    cout << "svm pingpong." << endl
         << "Multiple graphics cards talk to each other." << endl
         << "This requires atomic operations on svm memory, and may not work on all graphics cards." << endl
         << "When specifying '--with_cpu=1', the cpu will be involved, as well." << endl
         << endl;

    init_params(argc, argv);
#if !defined(__cpp_lib_atomic_ref) || __cpp_lib_atomic_ref < 201806
    if (WITH_CPU())
    {
        std::cerr << "std::atomic_ref required for WITH_CPU" << std::endl;
        exit(EXIT_FAILURE);
    }
#endif

    pingpong<unsigned int>();
    // pingpong<debugtype<unsigned int>>();
}

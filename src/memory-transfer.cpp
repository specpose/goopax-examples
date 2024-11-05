// @@@ CONVERT_TYPES_IGNORE @@@
#include <cassert>
#include <chrono>
#include <goopax>
#include <goopax_extra/param.hpp>
#include <iostream>
using namespace goopax;
using namespace std;
using namespace chrono;

PARAM<size_t> MEMSIZE("size", "buffer size in MB");

void print(const string& s, duration<double> dt)
{
    cout << s << " : time=" << dt.count() << " s, transfer rate: " << MEMSIZE() / 1024.0 / dt.count() << " GB/s"
         << endl;
}

int main(int argc, char** argv)
{
    init_params(argc, argv);

    for (goopax_device device : devices())
    {
        cout << "\nUsing device " << device.name() << endl;
        using T = int;

        vector<T> A(MEMSIZE() * (1 << 20) / sizeof(T));
        buffer<T> B(device, A.size());
        buffer<T> C(device, A.size());
        vector<T> D(A.size());
        vector<T> E(A.size());

        for (int LOOP = 0; LOOP < 5; ++LOOP)
        {
            cout << endl;
            for (size_t k = 0; k < A.size(); ++k)
            {
                A[k] = k + LOOP;
            }

            const auto ta = high_resolution_clock::now();
            B.copy_from_host(&A[0]);
            const auto tb = high_resolution_clock::now();
            C.copy(B);
            {
                T tmp;
                C.copy_to_host(&tmp, 0, 1);
            }

            device.wait_all();
            const auto tc = high_resolution_clock::now();
            C.copy_to_host(&D[0]);
            const auto td = high_resolution_clock::now();
            std::copy(D.begin(), D.end(), E.begin());
            const auto te = high_resolution_clock::now();

            print("host -> GPU ", tb - ta);
            print("GPU  -> GPU ", tc - tb);
            print("GPU  -> host", td - tc);
            print("host -> host", te - td);

            cout << "Testing result... ";
            for (size_t k = 0; k < A.size(); ++k)
            {
                assert(A[k] == E[k]);
            }
            cout << "ok" << endl;
        }
    }
}

/**
\example cuda-interop.cu

Cuda interoperability example program
*/

#include <assert.h>
#include <goopax>
#include <iostream>
#include <vector>

using std::cerr;
using std::cout;
using std::endl;

template<typename T>
std::ostream& operator<<(std::ostream& s, const std::vector<T>& v)
{
    s << "(";
    for (int k = 0; k < v.size(); ++k)
    {
        if (k != 0)
            s << ",";
        s << v[k];
    }
    s << ")";
    return s;
}

void check_cuda(cudaError_t err)
{
    if (err != cudaSuccess)
    {
        cerr << "CUDA error: " << cudaGetErrorString(err) << endl;
        abort();
    }
}

__global__ void inc_cuda(float* C, int numElements)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i < numElements)
    {
        C[i] += 1;
    }
}

int main()
{
    std::vector<float> h_A(100);

    goopax::goopax_device device = goopax::devices(goopax::env_CUDA)[0];

    goopax::kernel inc_goopax(device, [](goopax::resource<float>& A) {
        using namespace goopax;
        gpu_for_global(0, A.size(), [&](gpu_uint k) { A[k] += 1; });
    });

    for (unsigned int k = 0; k < h_A.size(); ++k)
    {
        h_A[k] = k;
    }
    cout << "starting with h_A=" << h_A << endl;

    cout << "Allocating cuda device memory." << endl;
    float* d_A = nullptr;
    check_cuda(cudaMalloc((void**)&d_A, h_A.size() * sizeof(h_A[0])));

    cout << "Copying to cuda device memory." << endl;
    check_cuda(cudaMemcpy(d_A, h_A.data(), h_A.size() * sizeof(float), cudaMemcpyHostToDevice));

    cout << "Sharing memory region with goopax" << endl;
    goopax::buffer<float> A_goopax = goopax::buffer<float>::create_from_cuda(device, d_A, h_A.size());

    cout << "Calling cuda kernel" << endl;
    int threadsPerBlock = 256;
    int blocksPerGrid = 16;
    inc_cuda<<<blocksPerGrid, threadsPerBlock>>>(d_A, h_A.size());
    check_cuda(cudaGetLastError());

    cout << "Calling goopax kernel" << endl;
    inc_goopax(A_goopax);

    cout << "Copying back to host memory." << endl;
    A_goopax.copy_to_host(h_A.data());

    cout << "now: h_A=" << h_A << endl;

    cout << "Checking result." << endl;
    for (unsigned int k = 0; k < h_A.size(); ++k)
    {
        assert(h_A[k] == k + 2);
    }

    cout << "Freeing cuda memory" << endl;
    check_cuda(cudaFree(d_A));
}

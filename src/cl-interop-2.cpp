// @@@ CONVERT_TYPES_IGNORE @@@

/*

  OpenCL interoperability example program 2

  In this example, a compute environment is set up by OpenCL.
  An OpenCL buffer is allocated and used by OpenCL and GOOPAX.

 */

#define CL_HPP_ENABLE_EXCEPTIONS 1
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#include "common/cl2.hpp"
#include <goopax_cl>
#include <goopax_extra/output.hpp>
#include <iostream>

using namespace goopax;
using namespace std;

int main()
{
    vector<cl::Platform> all_platforms;
    cl::Platform::get(&all_platforms);
    if (all_platforms.size() == 0)
    {
        cout << "No platforms found. Check OpenCL installation!\n";
        exit(EXIT_FAILURE);
    }
    cl::Platform platform = all_platforms[0];

    // get default device of the default platform
    vector<cl::Device> all_devices;
    platform.getDevices(CL_DEVICE_TYPE_GPU, &all_devices);
    if (all_devices.size() == 0)
    {
        cout << "No devices found. Check OpenCL installation!\n";
        exit(EXIT_FAILURE);
    }
    cl::Device device_cl = all_devices[0];
    cl::Context context({ device_cl });

    cl::Program::Sources sources;

    // kernel that increments each element in A.
    string kernel_code = "void kernel inc(global int* A)  "
                         "{                               "
                         "  A[get_global_id(0)] += 1;     "
                         "}                               ";
    sources.push_back({ kernel_code.c_str(), kernel_code.length() });

    cl::Program program(context, sources);
    if (program.build({ device_cl }) != CL_SUCCESS)
    {
        cout << " Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device_cl) << "\n";
        exit(EXIT_FAILURE);
    }

    cl::CommandQueue queue(context, device_cl);

    vector<int> A_cpu = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    // create OpenCL buffer
    cl::Buffer A_cl(context, CL_MEM_READ_WRITE, sizeof(int) * A_cpu.size());

    cout << "Writing data to OpenCL buffer:" << endl;
    for (auto i : A_cpu)
    {
        cout << i << " ";
    }
    cout << endl;

    queue.enqueueWriteBuffer(A_cl, CL_TRUE, 0, sizeof(int) * A_cpu.size(), &A_cpu[0]);

    cl::Kernel opencl_kernel = cl::Kernel(program, "inc");
    opencl_kernel.setArg(0, A_cl);
    queue.enqueueNDRangeKernel(opencl_kernel, cl::NullRange, cl::NDRange(A_cpu.size()), cl::NullRange);
    queue.finish();

    vector<int> result(A_cpu.size());
    queue.enqueueReadBuffer(A_cl, CL_TRUE, 0, sizeof(int) * result.size(), &result[0]);
    cout << "\nAfter OpenCL increase:" << endl;
    for (auto i : result)
    {
        cout << i << " ";
    }
    cout << endl;

    // Creating GOOPAX environment from OpenCL environment
    // goopax_env_from_cl ENV(argc, argv, queue());
    goopax_device device = get_device_from_cl_queue(queue());

    // sharing the buffer with the OpenCL buffer
    auto A_goopax = buffer<int>::create_from_cl(device, A_cl());

    kernel Goopax_kernel(A_goopax.get_device(),
                         [](resource<int>& A) { gpu_for_global(0, A.size(), [&](gpu_uint k) { ++A[k]; }); });

    // Now calling GOOPAX kernel.
    Goopax_kernel(A_goopax);
    cout << "\nAfter goopax increase:" << endl << A_goopax << endl;

    return 0;
}

// @@@ CONVERT_TYPES_IGNORE @@@

/*

  OpenCL interoperability example program 1

  In this example, a compute environment is set up by GOOPAX.
  A GOOPAX buffer is allocated and used by OpenCL and GOOPAX.

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

void inc_cl(buffer<int>& A)
{
    cl::Device device = goopax::get_cl_cxx_device(A.get_device());
    cl::Context context = goopax::get_cl_cxx_context(A.get_device());

    cl::Program::Sources sources;

    // OpenCL kernel that increments each element in A.
    string kernel_code = "void kernel inc(global int* A)  "
                         "{                               "
                         "  A[get_global_id(0)] += 1;     "
                         "}                               ";
    sources.push_back({ kernel_code.c_str(), kernel_code.length() });

    cl::Program program(context, sources);
    if (program.build({ device }) != CL_SUCCESS)
    {
        cout << " Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << endl;
        exit(EXIT_FAILURE);
    }

    cl::CommandQueue queue = goopax::get_cl_cxx_queue(A.get_device());

    cl::Buffer A_cl = get_cl_cxx_buf(A);

    cl::Kernel opencl_kernel = cl::Kernel(program, "inc");
    opencl_kernel.setArg(0, A_cl);
    queue.enqueueNDRangeKernel(opencl_kernel, cl::NullRange, cl::NDRange(A.size()), cl::NullRange);
    queue.finish();
}

void inc_goopax(buffer<int>& A)
{
    kernel Inc(A.get_device(), [](resource<int>& A) { gpu_for_global(0, A.size(), [&](gpu_uint k) { A[k] += 1; }); });

    Inc(A);
}

int main()
{
    goopax_device device = default_device(env_CL);

    buffer<int> A(device, std::vector{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 });

    cout << "\noriginal buffer:" << endl << A << endl;

    inc_cl(A);

    cout << "After OpenCL increase:" << endl << A << endl;

    inc_goopax(A);

    cout << "After goopax increase:" << endl << A << endl;

    return 0;
}

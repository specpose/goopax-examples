// @@@ CONVERT_TYPES_IGNORE @@@

#include <goopax>
#include <goopax_extra/output.hpp>
using namespace goopax;
using namespace goopax::debug;
using namespace types;

/*
  Provoking and detecting a race condition. This program will abort with an error message.
  Uncommenting "L.barrier();" will repair it.
 */

int main()
{
    std::cout << "Checking for race conditions." << std::endl;

    goopax_device device = default_device(env_CPU);

    if (!device.valid())
    {
        std::cout << "No CPU device found." << std::endl;
        return 0;
    }

    buffer<Tfloat> a(device, 64);
    a.fill(0);

    kernel Program(
        device,
        [](resource<Tfloat>& A) {
            local_mem<Tfloat> L(local_size());

            L[local_id()] = global_id();
            // L.barrier();
            A[global_id()] = L[(local_id() + 1) % local_size()];
        },
        64,
        64);

    Program(a);

    std::cout << "a=" << a << std::endl;
}

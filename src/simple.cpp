#include "common/output.hpp"
#include <goopax>

using namespace goopax;
using namespace std;

int main()
{
    goopax_device device = default_device(env_ALL);

    kernel foo(device, [](resource<int>& A) { for_each_global(A.begin(), A.end(), [](auto& a) { a = global_id(); }); });

    buffer<int> A(device, 1000);
    foo(A);

    cout << "A=" << A << endl;
}

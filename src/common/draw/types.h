#pragma once

#include <goopax>

#if __has_include(<Eigen/Eigen>)
#include <Eigen/Eigen>

#if !EIGEN_VERSION_AT_LEAST(3, 3, 90)
namespace Eigen
{
template<typename T, size_t N>
using Vector = Matrix<T, N, 1>;

template<typename T>
using Vector3 = Vector<T, 3>;

template<typename T>
using VectorX = Eigen::Matrix<T, Eigen::Dynamic, 1>;

template<typename T>
using MatrixX = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>;

}
#endif

namespace goopax
{
template<class T, int... N>
struct goopax_struct_type<Eigen::Matrix<T, N...>>
{
    using type = T;
};

template<class T, int... N, class X>
struct goopax_struct_changetype<Eigen::Matrix<T, N...>, X>
{
    using type = Eigen::Matrix<typename goopax_struct_changetype<T, X>::type, N...>;
};
}

#endif

#if GOOPAX_DEBUG
using namespace goopax::debug::types;
template<typename T>
using Tdebugtype = goopax::debugtype<T>;
#else
using namespace goopax::release::types;
template<typename T>
using Tdebugtype = T;
#endif

#ifdef __STDCPP_FLOAT16_T__
using Thalf = Tdebugtype<std::float16_t>;
#elif __has_include(<Eigen/Eigen>)
using Thalf = Tdebugtype<Eigen::half>;
#endif

#ifdef __STDCPP_BFLOAT16_T__
using Tbfloat16 = Tdebugtype<std::bfloat16_t>;
#elif __has_include(<Eigen/Eigen>)
using Tbfloat16 = Tdebugtype<Eigen::bfloat16>;
#endif

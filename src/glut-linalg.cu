#if _DEBUG
#define DEBUG 1
#endif
#include <array>
#include <iostream>
#include <iterator>
//#include <stdio.h>
#include <vector>

#include <goopax>

/* Col Major 3D*/
template<typename T>
struct Matrix3d
{
    // public:
    static Matrix3d identity();

    static T det(const Matrix3d& matrix);

    static bool isSingular(const Matrix3d& matrix);

    static Matrix3d scale(const T& x, const T& y, const T& z);

    static Matrix3d rotate(double deg, const T& x, const T& y, const T& z);

    static Matrix3d translate(const T& x, const T& y, const T& z);
    double static _radToDeg(double rad);
    double static _degToRad(double deg);

    // private:
    static Matrix3d mul(const Matrix3d& a, const Matrix3d& b);
    T x1, x2, x3, x4, y1, y2, y3, y4, z1, z2, z3, z4, w1, w2, w3, w4;
};

template<typename T>
class Stack3d : public std::vector<Matrix3d<T>>
{

public:
    using std::vector<Matrix3d<T>>::vector;

    void identity();
    void scale(const T& x, const T& y, const T& z);
    void rotate(double deg, const T& x, const T& y, const T& z);
    void translate(const T& x, const T& y, const T& z);
};

template<typename pod_T, typename tf = std::enable_if_t<std::is_pod<pod_T>::value>>
class Vectors
{
public:
    Vectors(pod_T* ptr, std::size_t size)
        : _size(size)
        , storage(ptr)
    {
    }
    virtual ~Vectors()
    {
    }

    template<typename T>
    void apply(const Stack3d<T>& stack);
    template<typename T>
    void apply(const Matrix3d<T>& matrix);

    pod_T& operator[](std::size_t i)
    {
        return storage[i];
    }
    std::size_t size()
    {
        return _size;
    }

protected:
    pod_T* storage;

private:
    std::size_t _size;
};
template<typename pod_T>
class CuVectors : public Vectors<pod_T>
{
public:
    using Vectors<pod_T>::Vectors;
    template<typename T>
    void apply(const Stack3d<T>& stack);
    template<typename T>
    void apply(const Matrix3d<T>& matrix);
};
template<typename pod_T>
class GooVectors : public Vectors<pod_T>
{
public:
    GooVectors(pod_T* ptr, std::size_t size)
        : Vectors<pod_T>(ptr, size)
        , dev(goopax::default_device(goopax::env_CUDA))
        , storage_(dev, size, storage)
    {
    }
    template<typename T>
    void apply(const Stack3d<T>& stack);
    template<typename T>
    void apply(const Matrix3d<T>& matrix);

protected:
    goopax::goopax_device dev;
    goopax::buffer<pod_T> storage_;
};

#include <algorithm>
#include <cmath>
#include <stdexcept>

// not available on windows:
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

template<typename pod_T, typename tf>
template<typename T>
void Vectors<pod_T, tf>::apply(const Stack3d<T>& stack)
{
    auto first = std::begin(stack);
    if (first != std::end(stack))
    {
#if DEBUG
        if (*first == Matrix3d<T>::identity())
            throw std::logic_error("multiplying identity matrix has performance penalty.");
        if (Matrix3d<T>::isSingular(*first))
            throw std::logic_error("singular matrices are not invertible.");
#endif
        Matrix3d<T> all = *first;
        std::for_each(++first, std::end(stack), [&all](const Matrix3d<T>& m) {
#if DEBUG
            if (m == Matrix3d<T>::identity())
                throw std::logic_error("multiplying identity matrix has performance penalty.");
            if (Matrix3d<T>::isSingular(m))
                throw std::logic_error("singular matrices are not invertible.");
#else
            all = Matrix3d<T>::mul(all, m);
#endif
        });
        apply(all);
    }
}
template<typename pod_T>
template<typename T>
void CuVectors<pod_T>::apply(const Stack3d<T>& stack)
{
    auto first = std::begin(stack);
    if (first != std::end(stack))
    {
        Matrix3d<T> all = *first;
        std::for_each(++first, std::end(stack), [&all](const Matrix3d<T>& m) { all = Matrix3d<T>::mul(all, m); });
        apply(all);
    }
}
template<typename pod_T>
template<typename T>
void GooVectors<pod_T>::apply(const Stack3d<T>& stack)
{
    auto first = std::begin(stack);
    if (first != std::end(stack))
    {
        Matrix3d<T> all = *first;
        std::for_each(++first, std::end(stack), [&all](const Matrix3d<T>& m) { all = Matrix3d<T>::mul(all, m); });
        apply(all);
    }
}

template<typename pod_T, typename tf>
template<typename T>
void Vectors<pod_T, tf>::apply(const Matrix3d<T>& m)
{
    std::transform(&this->storage[0], &this->storage[0] + this->size(), &this->storage[0], [&m](pod_T& a) {
        pod_T value{ (m.x1 * a.x + m.x2 * a.y + m.x3 * a.z + m.x4 * 1),
                     (m.y1 * a.x + m.y2 * a.y + m.y3 * a.z + m.y4 * 1),
                     (m.z1 * a.x + m.z2 * a.y + m.z3 * a.z + m.z4 * 1) };
        return value;
    });
}
template<typename T, typename pod_T>
__global__ void cuMatVec(Matrix3d<T>* mu, pod_T* v)
{
    int i = threadIdx.x;
    pod_T a = v[i];
    Matrix3d<T> m = mu[0];
    pod_T value{ (m.x1 * a.x + m.x2 * a.y + m.x3 * a.z + m.x4 * 1),
                 (m.y1 * a.x + m.y2 * a.y + m.y3 * a.z + m.y4 * 1),
                 (m.z1 * a.x + m.z2 * a.y + m.z3 * a.z + m.z4 * 1) };
    v[i] = value;
}
template<typename pod_T>
template<typename T>
void CuVectors<pod_T>::apply(const Matrix3d<T>& matrix)
{
    Matrix3d<T>* matrix_;
    pod_T* vectors_;
    cudaMalloc(&matrix_, sizeof(matrix));
    cudaMalloc(&vectors_, this->size() * sizeof(pod_T));
    cudaMemcpy(matrix_, &matrix, sizeof(matrix), cudaMemcpyHostToDevice);
    cudaMemcpy(vectors_, this->storage, this->size() * sizeof(pod_T), cudaMemcpyHostToDevice);
    cuMatVec<<<1, this->size()>>>(matrix_, vectors_);
    cudaMemcpy(this->storage, vectors_, this->size() * sizeof(pod_T), cudaMemcpyDeviceToHost);
    cudaFree(vectors_);
    cudaFree(matrix_);
}
template<typename T, typename pod_T>
void gooMatVec(const Matrix3d<T>& m, pod_T& a)
{
    typename std::remove_reference<decltype(a)>::type value{};
    // auto value = goopax::make_gpu<pod_T>{};
    // typename goopax::goopax_struct_changetype< pod_T, typename goopax::goopax_struct_type<pod_T>::type >::type
    // value{};
    value.x = (m.x1 * a.x + m.x2 * a.y + m.x3 * a.z + m.x4 * 1);
    value.y = (m.y1 * a.x + m.y2 * a.y + m.y3 * a.z + m.y4 * 1);
    value.z = (m.z1 * a.x + m.z2 * a.y + m.z3 * a.z + m.z4 * 1);
    a = value;
}
template<typename pod_T>
template<typename T>
void GooVectors<pod_T>::apply(const Matrix3d<T>& m)
{
    auto k = goopax::kernel(dev, [&](goopax::resource<pod_T>& v) {
        goopax::gpu_for_global(0, this->size(), [&m, &v](goopax::gpu_uint i) { gooMatVec(m, v[i]); });
    });
    k(storage_);
}

template<typename T>
Matrix3d<T> Matrix3d<T>::identity()
{
    auto m = Matrix3d<T>{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    return m;
}

template<typename T>
T Matrix3d<T>::det(const Matrix3d<T>& m)
{
    // auto det = x1 * y2 * z3 + y1 * z2 * x3 + z1 * x2 * y3 - x1 * z2 * x3 - y1 * x2 * z3 - z1 * y2 * x3;
    auto det =
        -m.x2 * m.y3 * m.z4 * m.w1 + m.x2 * m.y4 * m.z3 * m.w1 + m.x3 * m.y2 * m.z4 * m.w1 - m.x3 * m.y4 * m.z2 * m.w1
        - m.x4 * m.y2 * m.z3 * m.w1 + m.x4 * m.y3 * m.z2 * m.w1 + m.x1 * m.y3 * m.z4 * m.w2 - m.x1 * m.y4 * m.z3 * m.w2
        - m.x3 * m.y1 * m.z4 * m.w2 + m.x3 * m.y4 * m.z1 * m.w2 + m.x4 * m.y1 * m.z3 * m.w2 - m.x4 * m.y3 * m.z1 * m.w2
        - m.w3 * m.x1 * m.y2 * m.z4 + m.w3 * m.x1 * m.y4 * m.z2 + m.w3 * m.x2 * m.y1 * m.z4 - m.w3 * m.x2 * m.y4 * m.z1
        - m.w3 * m.x4 * m.y1 * m.z2 + m.w3 * m.x4 * m.y2 * m.z1 + m.w4 * m.x1 * m.y2 * m.z3 - m.w4 * m.x1 * m.y3 * m.z2
        - m.w4 * m.x2 * m.y1 * m.z3 + m.w4 * m.x2 * m.y3 * m.z1 + m.w4 * m.x3 * m.y1 * m.z2 - m.w4 * m.x3 * m.y2 * m.z1;
    return det;
}

template<typename T>
bool Matrix3d<T>::isSingular(const Matrix3d<T>& matrix)
{
#if DEBUG
    return (Matrix3d<T>::det(matrix) == 0);
#else
    return false;
#endif
}

template<typename T>
Matrix3d<T> Matrix3d<T>::scale(const T& x, const T& y, const T& z)
{
    auto m = Matrix3d<T>{ x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1 };
    return m;
}

/* template<typename T>
Matrix2d<T> Matrix2d<T>::rotate(double deg)
{
    T rad = _degToRad(deg);
    auto m = Matrix2d<T>{ cos(rad), sin(rad), 0, -sin(rad), cos(rad), 0, 0, 0, 1 };
    return m;
}*/

template<typename T>
Matrix3d<T> Matrix3d<T>::rotate(double deg, const T& x, const T& y, const T& z)
{
    double rad = _degToRad(deg);
    double f = 1 - cos(rad);
    auto m = Matrix3d<T>{ x * x * f + cos(rad),
                          y * x * f + z * sin(rad),
                          z * x * f - y * sin(rad),
                          0,
                          x * y * f - z * sin(rad),
                          y * y * f + cos(rad),
                          z * y * f + x * sin(rad),
                          0,
                          x * z * f + y * sin(rad),
                          y * z * f - x * sin(rad),
                          z * z * f + cos(rad),
                          0,
                          0,
                          0,
                          0,
                          1 };
    return m;
}

template<typename T>
Matrix3d<T> Matrix3d<T>::translate(const T& x, const T& y, const T& z)
{
    auto m = Matrix3d<T>{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1 };
    return m;
}

template<typename T>
double Matrix3d<T>::_radToDeg(double rad)
{
    return rad * (180.0 / M_PI);
}
template<typename T>
double Matrix3d<T>::_degToRad(double deg)
{
    return deg / (180.0 / M_PI);
}

template<typename T>
bool operator==(const Matrix3d<T>& a, const Matrix3d<T>& b)
{
    return a.x1 == b.x1 && a.x2 == b.x2 && a.x3 == b.x3 && a.x4 == b.x4 && a.y1 == b.y1 && a.y2 == b.y2 && a.y3 == b.y3
           && a.y4 == b.y4 && a.z1 == b.z1 && a.z2 == b.z2 && a.z3 == b.z3 && a.z4 == b.z4 && a.w1 == b.w1
           && a.w2 == b.w2 && a.w3 == b.w3 && a.w4 == b.w4;
}

/* template<typename T>
Matrix2d<T> Matrix2d<T>::mul(const Matrix2d<T>& a, const Matrix2d<T>& b)
{
    auto m = Matrix2d<T>{
        a.x1 * b.x1 + a.y1 * b.x2 + a.z1 * b.x3, // c00
        a.x1 * b.y1 + a.y1 * b.y2 + a.z1 * b.y3, // c01
        a.x1 * b.z1 + a.y1 * b.z2 + a.z1 * b.z3, // c02
        a.x2 * b.x1 + a.y2 * b.x2 + a.z2 * b.x3, // c10
        a.x2 * b.y1 + a.y2 * b.y2 + a.z2 * b.y3, // c11
        a.x2 * b.z1 + a.y2 * b.z2 + a.z2 * b.z3, // c12
        a.x3 * b.x1 + a.y3 * b.x2 + a.z3 * b.x3, // c20
        a.x3 * b.y1 + a.y3 * b.y2 + a.z3 * b.y3, // c21
        a.x3 * b.z1 + a.y3 * b.z2 + a.z3 * b.z3  // c22
    };
    return m;
}*/

template<typename T>
Matrix3d<T> Matrix3d<T>::mul(const Matrix3d<T>& a, const Matrix3d<T>& b)
{
    auto m = Matrix3d<T>{
        a.x1 * b.x1 + a.y1 * b.x2 + a.z1 * b.x3 + a.w1 * b.x4, a.x1 * b.y1 + a.y1 * b.y2 + a.z1 * b.y3 + a.w1 * b.y4,
        a.x1 * b.z1 + a.y1 * b.z2 + a.z1 * b.z3 + a.w1 * b.z4, a.x1 * b.w1 + a.y1 * b.w2 + a.z1 * b.w3 + a.w1 * b.w4,
        a.x2 * b.x1 + a.y2 * b.x2 + a.z2 * b.x3 + a.w2 * b.x4, a.x2 * b.y1 + a.y2 * b.y2 + a.z2 * b.y3 + a.w2 * b.y4,
        a.x2 * b.z1 + a.y2 * b.z2 + a.z2 * b.z3 + a.w2 * b.z4, a.x2 * b.w1 + a.y2 * b.w2 + a.z2 * b.w3 + a.w2 * b.w4,
        a.x3 * b.x1 + a.y3 * b.x2 + a.z3 * b.x3 + a.w3 * b.x4, a.x3 * b.y1 + a.y3 * b.y2 + a.z3 * b.y3 + a.w3 * b.y4,
        a.x3 * b.z1 + a.y3 * b.z2 + a.z3 * b.z3 + a.w3 * b.z4, a.x3 * b.w1 + a.y3 * b.w2 + a.z3 * b.w3 + a.w3 * b.w4
    };
    return m;
}

template<typename T>
void Stack3d<T>::identity()
{
    this->clear();
}

template<typename T>
void Stack3d<T>::scale(const T& x, const T& y, const T& z)
{
    if (!((1 == x) && (1 == y) && (1 == z)))
        this->push_back(Matrix3d<T>::scale(x, y, z));
}

template<typename T>
void Stack3d<T>::rotate(double deg, const T& x, const T& y, const T& z)
{
    if (!(deg == 0))
        this->push_back(Matrix3d<T>::rotate(deg, x, y, z));
}

template<typename T>
void Stack3d<T>::translate(const T& x, const T& y, const T& z)
{
    if (!((0 == x) && (0 == y) && (0 == z)))
        this->push_back(Matrix3d<T>::translate(x, y, z));
}

// POD
template<typename T>
struct Vector3d
{
    T x, y, z;
};
GOOPAX_PREPARE_STRUCT(Vector3d)
/* namespace goopax
{
    template<typename T>
    struct goopax_struct_type<Vector3d<T>>
    {
        using type = T;
    };
    template<typename T, typename X>
    struct goopax_struct_changetype<Vector3d<T>, X>
    {
        using type = Vector3d< typename goopax_struct_changetype<T, X>::type >;
    };
}*/
template<typename T>
Vector3d<T> operator+(const Vector3d<T>& a, const Vector3d<T>& b)
{
    auto c = Vector3d<T>{};
    c.x = a.x + b.x;
    c.y = a.y + b.y;
    c.z = a.z + b.z;
    return c;
};
template<typename T>
std::ostream& operator<<(std::ostream& os, const Vector3d<T>& v)
{
    os << "(";
    os << v.x << ",";
    os << v.y;
    os << ")" << std::endl;
    return os;
}

int main()
{
    using T = double;
    using D = std::vector<Vector3d<T>>;
    using C1 = Vectors<Vector3d<T>>;
#if DEBUG
    auto st2 = Stack3d<T>();
    // st2.push_back(Matrix3d<T>{ 1, 2, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
    // st2.push_back(Matrix3d<T>{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 });
    auto test = D(1);
    test[0] = Vector3d<T>{ 1, 0, 0 };
    auto test_1 = C1(test.data(), std::size(test));
    test_1.apply(st2);
#endif
    using C2 = CuVectors<Vector3d<T>>;
    using C3 = GooVectors<Vector3d<T>>;
    auto st = Stack3d<T>();
    st.identity();
    // st.scale(2, 2);

    auto sq = D(5);
    T square = 0.5;
    sq[0] = Vector3d<T>{ square, square, 0 };
    sq[1] = Vector3d<T>{ -square, square, 0 };
    sq[2] = Vector3d<T>{ -square, -square, 0 };
    sq[3] = Vector3d<T>{ square, -square, 0 };
    sq[4] = Vector3d<T>{ square, square, 0 };
    auto sq_copy = sq;
    auto sq_1 = C1(sq_copy.data(), std::size(sq_copy));
    sq_1.apply(st);
    std::for_each(&sq_1[0], &sq_1[0] + sq_1.size(), [](auto& v) { std::cout << v << ","; });
    sq_copy = sq;
    auto sq_2 = C2(sq_copy.data(), std::size(sq_copy));
    sq_2.apply(st);
    std::for_each(&sq_2[0], &sq_2[0] + sq_2.size(), [](auto& v) { std::cout << v << ","; });
    sq_copy = sq;
    auto sq_3 = C3(sq_copy.data(), std::size(sq_copy));
    sq_3.apply(st);
    std::for_each(&sq_3[0], &sq_3[0] + sq_3.size(), [](auto& v) { std::cout << v << ","; });
    // drawVectors(sq);
    std::cout << std::endl;

    auto v1 = Vector3d<T>{ 1, 0, 0 };
    auto v2 = Vector3d<T>{ sqrt(2) / 2, sqrt(2) / 2, 0 };
    auto v3 = Vector3d<T>{ 0, 1, 0 };
    auto _data_sink = D(3);
    _data_sink[0] = v1;
    _data_sink[1] = v1 + v2;
    _data_sink[2] = v1 + v2 + v3;

    // auto last_pair = _data_sink.back();//multiple tracks
    auto last_pair = _data_sink;
    auto left = last_pair;

    auto middle1 = _data_sink[0];
    auto middle2 = _data_sink[1];
    double angle2 = Matrix3d<T>::_radToDeg(atan2((middle2.x - middle1.y), (middle2.x - middle1.y)));
    // glRotatef(-angle2, 0.0f, 0.0f, 1.0f);
    // st.rotate(-angle2, 0, 0, 1);
    st.rotate(-angle2, 0, 0, 1);
    st.translate(-middle1.x, -middle1.y, -middle1.z);

    // middle marker
    auto mid = D(2);
    mid[0] = Vector3d<T>{ middle1.x, middle1.y, middle1.z };
    mid[1] = Vector3d<T>{ middle2.x, middle2.y, middle2.z };
    auto mid_copy = mid;
    auto mid_1 = C1(mid_copy.data(), std::size(mid_copy));
    mid_1.apply(st);
    std::for_each(&mid_1[0], &mid_1[0] + mid_1.size(), [](auto& v) { std::cout << v << ","; });
    mid_copy = mid;
    auto mid_2 = C2(mid_copy.data(), std::size(mid_copy));
    mid_2.apply(st);
    std::for_each(&mid_2[0], &mid_2[0] + mid_2.size(), [](auto& v) { std::cout << v << ","; });
    mid_copy = mid;
    auto mid_3 = C3(mid_copy.data(), std::size(mid_copy));
    mid_3.apply(st);
    std::for_each(&mid_3[0], &mid_3[0] + mid_3.size(), [](auto& v) { std::cout << v << ","; });
    // drawVectors(mid);
    std::cout << std::endl;

    auto vecs = D(left.size() + 1);
    vecs[0] = Vector3d<T>{ 0, 0, 0 };
    for (int i = 1; i < vecs.size(); i++)
    {
        vecs[i] = Vector3d<T>{ left[i - 1].x, left[i - 1].y, left[i - 1].z };
    }
    auto vecs_copy = vecs;
    auto vecs_1 = C1(vecs_copy.data(), std::size(vecs_copy));
    vecs_1.apply(st);
    std::for_each(&vecs_1[0], &vecs_1[0] + vecs_1.size(), [](auto& v) { std::cout << v << ","; });
    vecs_copy = vecs;
    auto vecs_2 = C2(vecs_copy.data(), std::size(vecs_copy));
    vecs_2.apply(st);
    std::for_each(&vecs_2[0], &vecs_2[0] + vecs_2.size(), [](auto& v) { std::cout << v << ","; });
    vecs_copy = vecs;
    auto vecs_3 = C3(vecs_copy.data(), std::size(vecs_copy));
    vecs_3.apply(st);
    std::for_each(&vecs_3[0], &vecs_3[0] + vecs_3.size(), [](auto& v) { std::cout << v << ","; });
    // drawVectors(vecs);
    std::cout << std::endl;

#ifdef __CUDACC__
    std::cout << "Hello from CUDA!" << std::endl;
#else
    std::cout << "CUDA is not here." << std::endl;
#endif

    return 0;
}
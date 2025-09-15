#include <array>
#include <iostream>
#include <iterator>
//#include <stdio.h>
#include <vector>

#include <goopax>

/* Col Major 2D*/
template<typename T>
struct Matrix2d
{
    // public:
    static Matrix2d identity();

    T det();

    bool isSingular();

    static Matrix2d scale(T x, T y);

    static Matrix2d rotate(T deg);

    static Matrix2d translate(T x, T y);
    double static _radToDeg(double rad);
    double static _degToRad(double deg);

    // private:
    static Matrix2d mul(const Matrix2d& a, const Matrix2d& b);
    T x1, x2, x3, y1, y2, y3, z1, z2, z3;
};

template<typename T>
class Stack2d : public std::vector<Matrix2d<T>>
{

public:
    using std::vector<Matrix2d<T>>::vector;

    void identity();
    void scale(T x, T y);
    void rotate(T deg);
    void translate(T x, T y);
};

template<typename pod_T>
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
    void apply(Stack2d<T>& stack);
    template<typename T>
    void apply(Matrix2d<T>& matrix);

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
    void apply(Stack2d<T>& stack);
    template<typename T>
    void apply(Matrix2d<T>& matrix);
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
    void apply(Stack2d<T>& stack);
    template<typename T>
    void apply(Matrix2d<T>& matrix);
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

template<typename pod_T>
template<typename T>
void Vectors<pod_T>::apply(Stack2d<T>& stack)
{
    auto first = std::begin(stack);
    if (first != std::end(stack))
    {
#if DEVELOPMENT
        if (*first == Matrix2d<T>::identity())
            throw std::logic_error("multiplying identity matrix has performance penalty.");
        if (first.isSingular())
            throw std::logic_error("singular matrices are not invertible.");
#endif
        Matrix2d<T> all = *first;
        std::for_each(++first, std::end(stack), [&all](Matrix2d<T>& m) {
#if DEVELOPMENT
            if (m == Matrix2d<T>::identity())
                throw std::logic_error("multiplying identity matrix has performance penalty.");
            if (m.isSingular())
                throw std::logic_error("singular matrices are not invertible.");
#else
            all = Matrix2d<T>::mul(all, m);
#endif
        });
        apply(all);
    }
}
template<typename pod_T>
template<typename T>
void CuVectors<pod_T>::apply(Stack2d<T>& stack)
{
    auto first = std::begin(stack);
    if (first != std::end(stack))
    {
        Matrix2d<T> all = *first;
        std::for_each(++first, std::end(stack), [&all](Matrix2d<T>& m) { all = Matrix2d<T>::mul(all, m); });
        apply(all);
    }
}
template<typename pod_T>
template<typename T>
void GooVectors<pod_T>::apply(Stack2d<T>& stack)
{
    auto first = std::begin(stack);
    if (first != std::end(stack))
    {
        Matrix2d<T> all = *first;
        std::for_each(++first, std::end(stack), [&all](Matrix2d<T>& m) { all = Matrix2d<T>::mul(all, m); });
        apply(all);
    }
}

/*
template<typename pod_T>
template<typename T>
void Vectors<pod_T>::apply3(Matrix2d<T>& m)
{
    std::transform(&this->storage[0], &this->storage[0] + this->size(), &this->storage[0], [&m](pod_T& a) {
        pod_T value{ (m.x1 * a.x + m.x2 * a.y + m.x3 * a.z),
                     (m.y1 * a.x + m.y2 * a.y + m.y3 * a.z),
                     (m.z1 * a.x + m.z2 * a.y + m.z3 * a.z) };
        return value;
    });
}
*/
template<typename pod_T>
template<typename T>
void Vectors<pod_T>::apply(Matrix2d<T>& m)
{
    std::transform(&this->storage[0], &this->storage[0] + this->size(), &this->storage[0], [&m](pod_T& a) {
        pod_T value{ (m.x1 * a.x + m.x2 * a.y + m.x3 * 1), (m.y1 * a.x + m.y2 * a.y + m.y3 * 1) };
        return value;
    });
}
template<typename T, typename pod_T>
__global__ void cuMatVec(Matrix2d<T>* mu, pod_T* v)
{
    int i = threadIdx.x;
    // printf("v[%d] is (%f,%f) with
    // %f,%f,%f,%f,%f,%f,%f,%f,%f\n",i,v[i].x,v[i].y,m->x1,m->x2,m->x3,m->y1,m->y2,m->y3,m->z1,m->z2,m->z3);
    pod_T a = v[i];
    Matrix2d<T> m = mu[0];
    pod_T value{ (m.x1 * a.x + m.x2 * a.y + m.x3 * 1), (m.y1 * a.x + m.y2 * a.y + m.y3 * 1) };
    v[i] = value;
}
template<typename pod_T>
template<typename T>
void CuVectors<pod_T>::apply(Matrix2d<T>& matrix)
{
    Matrix2d<T>* matrix_;
    // printf("Sizeof Matrix2d is %d",sizeof(matrix));
    pod_T* vectors_;
    // printf("Sizeof this is %d", this->size() * sizeof(pod_T));
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
void gooMatVec(Matrix2d<T>& m, pod_T& a)
{
    typename std::remove_reference<decltype(a)>::type value{};
    // auto value = goopax::make_gpu<pod_T>{};
    // typename goopax::goopax_struct_changetype< pod_T, typename goopax::goopax_struct_type<pod_T>::type >::type
    // value{};
    value.x = (m.x1 * a.x + m.x2 * a.y + m.x3 * 1);
    value.y = (m.y1 * a.x + m.y2 * a.y + m.y3 * 1);
    a = value;
}
template<typename pod_T>
template<typename T>
void GooVectors<pod_T>::apply(Matrix2d<T>& m)
{
    auto k = goopax::kernel(dev, [&](goopax::resource<pod_T>& v) {
        goopax::gpu_for_global(0, this->size(), [&m, &v](goopax::gpu_uint i) { gooMatVec(m, v[i]); });
    });
    k(storage_);
}

template<typename T>
Matrix2d<T> Matrix2d<T>::identity()
{
    auto m = Matrix2d<T>{ 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    return m;
}

template<typename T>
T Matrix2d<T>::det()
{
    auto det = x1 * y2 * z3 + y1 * z2 * x3 + z1 * x2 * y3 - x1 * z2 * x3 - y1 * x2 * z3 - z1 * y2 * x3;
    return det;
}

template<typename T>
bool Matrix2d<T>::isSingular()
{
#if DEVELOPMENT
    return (det() == 0);
#else
    return false;
#endif
}

template<typename T>
Matrix2d<T> Matrix2d<T>::scale(T x, T y)
{
    auto m = Matrix2d<T>{ x, 0, 0, 0, y, 0, 0, 0, 1 };
    return m;
}

template<typename T>
Matrix2d<T> Matrix2d<T>::rotate(T deg)
{
    T rad = _degToRad(deg);
    auto m = Matrix2d<T>{ cos(rad), sin(rad), 0, -sin(rad), cos(rad), 0, 0, 0, 1 };
    return m;
}

template<typename T>
Matrix2d<T> Matrix2d<T>::translate(T x, T y)
{
    auto m = Matrix2d<T>{ 1, 0, 0, 0, 1, 0, x, y, 1 };
    return m;
}

template<typename T>
double Matrix2d<T>::_radToDeg(double rad)
{
    return rad * (180.0 / M_PI);
}
template<typename T>
double Matrix2d<T>::_degToRad(double deg)
{
    return deg / (180.0 / M_PI);
}

template<typename T>
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
}

template<typename T>
void Stack2d<T>::identity()
{
    this->clear();
}

template<typename T>
void Stack2d<T>::scale(T x, T y)
{
    if (!((1 == x) && (1 == y)))
        this->push_back(Matrix2d<T>::scale(x, y));
}

template<typename T>
void Stack2d<T>::rotate(T deg)
{
    if (!(deg == 0))
        this->push_back(Matrix2d<T>::rotate(deg));
}

template<typename T>
void Stack2d<T>::translate(T x, T y)
{
    if (!((0 == x) && (0 == y)))
        this->push_back(Matrix2d<T>::translate(x, y));
}

// POD
template<typename T>
struct Vector2d
{
    T x, y;
};
GOOPAX_PREPARE_STRUCT(Vector2d)
/* namespace goopax
{
    template<typename T>
    struct goopax_struct_type<Vector2d<T>>
    {
        using type = T;
    };
    template<typename T, typename X>
    struct goopax_struct_changetype<Vector2d<T>, X>
    {
        using type = Vector2d< typename goopax_struct_changetype<T, X>::type >;
    };
}*/
template<typename T>
Vector2d<T> operator+(const Vector2d<T>& a, const Vector2d<T>& b)
{
    auto c = Vector2d<T>{};
    c.x = a.x + b.x;
    c.y = a.y + b.y;
    return c;
};
template<typename T>
std::ostream& operator<<(std::ostream& os, const Vector2d<T>& v)
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
    using D = std::vector<Vector2d<T>>;
    using C1 = Vectors<Vector2d<T>>;
    using C2 = CuVectors<Vector2d<T>>;
    using C3 = GooVectors<Vector2d<T>>;
    auto st = Stack2d<T>();
    st.identity();
    // st.scale(2, 2);

    auto sq = D(5);
    T square = 0.5;
    sq[0] = Vector2d<T>{ square, square };
    sq[1] = Vector2d<T>{ -square, square };
    sq[2] = Vector2d<T>{ -square, -square };
    sq[3] = Vector2d<T>{ square, -square };
    sq[4] = Vector2d<T>{ square, square };
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

    auto v1 = Vector2d<T>{ 1, 0 };
    auto v2 = Vector2d<T>{ sqrt(2) / 2, sqrt(2) / 2 };
    auto v3 = Vector2d<T>{ 0, 1 };
    auto _data_sink = D(3);
    _data_sink[0] = v1;
    _data_sink[1] = v1 + v2;
    _data_sink[2] = v1 + v2 + v3;

    // auto last_pair = _data_sink.back();//multiple tracks
    auto last_pair = _data_sink;
    auto left = last_pair;

    auto middle1 = _data_sink[0];
    auto middle2 = _data_sink[1];
    double angle2 = Matrix2d<T>::_radToDeg(atan2((middle2.x - middle1.y), (middle2.x - middle1.y)));
    // glRotatef(-angle2, 0.0f, 0.0f, 1.0f);
    st.rotate(-angle2);
    st.translate(-middle1.x, -middle1.y);

    // middle marker
    auto mid = D(2);
    mid[0] = Vector2d<T>{ middle1.x, middle1.y };
    mid[1] = Vector2d<T>{ middle2.x, middle2.y };
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
    vecs[0] = Vector2d<T>{ 0, 0 };
    for (int i = 1; i < vecs.size(); i++)
    {
        vecs[i] = Vector2d<T>{ left[i - 1].x, left[i - 1].y };
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
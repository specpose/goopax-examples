#include <array>
#include <iostream>
#include <iterator>
#include <vector>

template<typename pod_T, std::size_t SIZE = 2>
class Vectors; // forward declaration! same as in real declaration below
template<typename pod_T, std::size_t SIZE = 2>
class CuVectors; // forward declaration! same as in real declaration below

/* Col Major 2D*/
template<typename T>
struct Matrix2d
{
    // template<typename pod_T, std::size_t SIZE>
    // friend class Vectors;
    // template<typename pod_T, std::size_t SIZE>
    // friend class CuVectors;

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

template<typename pod_T, std::size_t SIZE>
class Vectors // same as in forward declaration above!
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
    void apply3(Matrix2d<T>& matrix);
    template<typename T>
    void apply2(Matrix2d<T>& matrix);

    pod_T& operator[](std::size_t i)
    {
        return storage[i];
    }
    std::size_t size()
    {
        return _size;
    }

private:
    pod_T* storage;
    std::size_t _size;
};
template<typename pod_T, std::size_t SIZE>
class CuVectors // same as in forward declaration above!
{
public:
    CuVectors(pod_T* ptr, std::size_t size)
        : _size(size)
        , storage(ptr)
    {
    }
    virtual ~CuVectors()
    {
    }

    template<typename T>
    void apply(Stack2d<T>& stack);
    template<typename T>
    void apply3(Matrix2d<T>& matrix);
    template<typename T>
    void apply2(Matrix2d<T>& matrix);

    pod_T& operator[](std::size_t i)
    {
        return storage[i];
    }
    std::size_t size()
    {
        return _size;
    }

private:
    pod_T* storage;
    std::size_t _size;
};

#include <algorithm>
#include <cmath>
#include <stdexcept>

// not available on windows:
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

template<typename pod_T, std::size_t SIZE>
template<typename T>
void Vectors<pod_T, SIZE>::apply(Stack2d<T>& stack)
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
        if (SIZE < 2)
            throw std::logic_error("Matrix2d class does not work with vector dimensions lower than 2");
        else if (SIZE == 2)
            apply2(all);
        else
            apply3(all);
    }
}
template<typename pod_T, std::size_t SIZE>
template<typename T>
void CuVectors<pod_T, SIZE>::apply(Stack2d<T>& stack)
{
}

template<typename pod_T, std::size_t SIZE>
template<typename T>
void Vectors<pod_T, SIZE>::apply3(Matrix2d<T>& m)
{
    std::transform(&this->storage[0], &this->storage[0] + this->_size, &this->storage[0], [&m](pod_T& a) {
        pod_T value{ (m.x1 * a[0] + m.x2 * a[1] + m.x3 * a[2]),
                     (m.y1 * a[0] + m.y2 * a[1] + m.y3 * a[2]),
                     (m.z1 * a[0] + m.z2 * a[1] + m.z3 * a[2]) };
        return value;
    });
}
template<typename pod_T, std::size_t SIZE>
template<typename T>
void CuVectors<pod_T, SIZE>::apply3(Matrix2d<T>& matrix)
{
}

template<typename pod_T, std::size_t SIZE>
template<typename T>
void Vectors<pod_T, SIZE>::apply2(Matrix2d<T>& m)
{
    std::transform(&this->storage[0], &this->storage[0] + this->_size, &this->storage[0], [&m](pod_T& a) {
        pod_T value{ (m.x1 * a[0] + m.x2 * a[1] + m.x3 * 1), (m.y1 * a[0] + m.y2 * a[1] + m.y3 * 1) };
        return value;
    });
}
template<typename pod_T, std::size_t SIZE>
template<typename T>
void CuVectors<pod_T, SIZE>::apply2(Matrix2d<T>& matrix)
{
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

template<typename T>
struct Vector2d
{
    Vector2d()
    {
        std::fill(&vec[0], &vec[0] + _size, 0);
    }
    Vector2d(std::initializer_list<T> list)
    {
        auto index = std::copy(std::begin(list), std::end(list), &vec[0]);
        std::fill(index, &vec[0] + _size, 0);
    }
    T& operator[](std::size_t i)
    {
        return vec[i];
    }
    static const std::size_t _size = 2;
    T vec[_size];
};
template<typename T>
Vector2d<T> operator+(const Vector2d<T>& a, const Vector2d<T>& b)
{
    auto c = Vector2d<T>{};
    c.vec[0] = a.vec[0] + b.vec[0];
    c.vec[1] = a.vec[1] + b.vec[1];
    return c;
};
template<typename T>
std::ostream& operator<<(std::ostream& os, const Vector2d<T>& v)
{
    os << "(";
    os << v.vec[0] << ",";
    os << v.vec[1];
    os << ")" << std::endl;
    return os;
}

int main()
{
    using T = double;
    using D = std::vector<Vector2d<T>>;
    using C1 = Vectors<Vector2d<T>>;
    using C2 = CuVectors<Vector2d<T>>;
    using C3 = Vectors<Vector2d<T>>;
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
    sq_copy = sq;
    auto sq_2 = C2(sq_copy.data(), std::size(sq_copy));
    sq_2.apply(st);
    sq_copy = sq;
    auto sq_3 = C3(sq_copy.data(), std::size(sq_copy));
    sq_3.apply(st);
    // drawVectors(sq);
    std::for_each(&sq_3[0], &sq_3[0] + sq_3.size(), [](auto& v) { std::cout << v << ","; });
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
    double angle2 = Matrix2d<T>::_radToDeg(atan2((middle2[0] - middle1[1]), (middle2[0] - middle1[1])));
    // glRotatef(-angle2, 0.0f, 0.0f, 1.0f);
    st.rotate(-angle2);
    st.translate(-middle1[0], -middle1[1]);

    // middle marker
    auto mid = D(2);
    mid[0] = Vector2d<T>{ middle1[0], middle1[1] };
    mid[1] = Vector2d<T>{ middle2[0], middle2[1] };
    auto mid_copy = mid;
    auto mid_1 = C1(mid_copy.data(), std::size(mid_copy));
    mid_1.apply(st);
    mid_copy = mid;
    auto mid_2 = C1(mid_copy.data(), std::size(mid_copy));
    mid_2.apply(st);
    mid_copy = mid;
    auto mid_3 = C1(mid_copy.data(), std::size(mid_copy));
    mid_3.apply(st);
    // drawVectors(mid);
    std::for_each(&mid_3[0], &mid_3[0] + mid_3.size(), [](auto& v) { std::cout << v << ","; });
    std::cout << std::endl;

    auto vecs = D(left.size() + 1);
    vecs[0] = Vector2d<T>{ 0, 0 };
    for (int i = 1; i < vecs.size(); i++)
    {
        vecs[i] = Vector2d<T>{ left[i - 1][0], left[i - 1][1] };
    }
    auto vecs_copy = vecs;
    auto vecs_1 = C1(vecs_copy.data(), std::size(vecs_copy));
    vecs_1.apply(st);
    vecs_copy = vecs;
    auto vecs_2 = C1(vecs_copy.data(), std::size(vecs_copy));
    vecs_2.apply(st);
    vecs_copy = vecs;
    auto vecs_3 = C1(vecs_copy.data(), std::size(vecs_copy));
    vecs_3.apply(st);
    // drawVectors(vecs);
    std::for_each(&vecs_3[0], &vecs_3[0] + vecs_3.size(), [](auto& v) { std::cout << v << ","; });
    std::cout << std::endl;

#ifdef __CUDACC__
    std::cout << "Hello from CUDA!" << std::endl;
#else
    std::cout << "CUDA is not here." << std::endl;
#endif

    return 0;
}
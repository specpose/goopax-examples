#include <array>
#include <iostream>
#include <iterator>
#include <vector>

template<typename gpu_T, std::size_t SIZE = 2>
class Vectors; // forward declaration! same as in real declaration below

/* Col Major 2D*/
template<typename T>
class Matrix
{
    template<typename gpu_T, std::size_t SIZE>
    friend class Vectors;

public:
    Matrix(const Matrix& other) = default;
    Matrix(Matrix&& other) = default;
    explicit Matrix(T x1, T y1, T z1, T x2, T y2, T z2, T x3, T y3, T z3);
    std::array<T, 9>* data();

    Matrix& operator=(const Matrix& other) = default;
    bool operator!=(const Matrix& other)
    {
        return (elems != other.elems);
    }
    bool operator==(const Matrix& other)
    {
        return (elems == other.elems);
    }

    static Matrix identity();

    T det();

    bool isSingular();

    static Matrix scale(T x, T y);

    static Matrix rotate(T deg);

    static Matrix translate(T x, T y);
    double static _radToDeg(double rad);
    double static _degToRad(double deg);

private:
    static Matrix mul(const Matrix& a, const Matrix& b);

private:
    std::array<T, 9> elems;
};

template<typename T>
class Stack : public std::vector<Matrix<T>>
{

public:
    Stack();

    void identity();
    void scale(T x, T y);
    void rotate(T deg);
    void translate(T x, T y);
};

template<typename gpu_T, std::size_t SIZE>
class Vectors // same as in forward declaration above!
{
public:
    Vectors(std::size_t size)
        : _size(size)
        , storage(NULL)
    {
        storage = (gpu_T*)malloc(_size * sizeof(gpu_T));
    }
    ~Vectors()
    {
        // if (!storage == NULL)
        //    free((void*)storage);
    }

    template<typename T>
    void apply(Stack<T>& stack);
    template<typename T>
    void apply3(Matrix<T>& matrix);
    template<typename T>
    void apply2(Matrix<T>& matrix);

    gpu_T& operator[](std::size_t i)
    {
        return storage[i];
    }
    std::size_t size()
    {
        return _size;
    }

private:
    gpu_T* storage;
    std::size_t _size;
};

#include <algorithm>
#include <cmath>
#include <stdexcept>

// not available on windows:
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

template<typename gpu_T, std::size_t SIZE>
template<typename T>
void Vectors<gpu_T, SIZE>::apply(Stack<T>& stack)
{
    auto first = std::find_if(std::begin(stack), std::end(stack), [](Matrix<T>& m) { return !m.isSingular(); });
    if (first != std::end(stack))
    {
#if DEVELOPMENT
        if (*first == Matrix<T>::identity())
            throw std::logic_error(
                "multiplying identity matrix has performance penalty. Check before adding to stack.");
#endif
        Matrix<T> all = *first;
        std::for_each(++first, std::end(stack), [&all](Matrix<T>& m) {
#if DEVELOPMENT
            if (m == Matrix<T>::identity())
                throw std::logic_error(
                    "multiplying identity matrix has performance penalty. Check before adding to stack.");
#endif
            if (!m.isSingular())
                all = Matrix<T>::mul(all, m);
        });
        if (SIZE < 2)
            throw std::logic_error("Matrix class does not work with vector dimensions lower than 2");
        else if (SIZE == 2)
            apply2(all);
        else
            apply3(all);
    }
}

template<typename gpu_T, std::size_t SIZE>
template<typename T>
void Vectors<gpu_T, SIZE>::apply3(Matrix<T>& matrix)
{
    auto& e = matrix.elems;
    std::transform(&this->storage[0], &this->storage[0] + this->_size, &this->storage[0], [&e](gpu_T& a) {
        gpu_T value{ (e[0] * a[0] + e[1] * a[1] + e[2] * a[2]),
                     (e[3] * a[0] + e[4] * a[1] + e[5] * a[2]),
                     (e[6] * a[0] + e[7] * a[1] + e[8] * a[2]) };
        return value;
    });
}
template<typename gpu_T, std::size_t SIZE>
template<typename T>
void Vectors<gpu_T, SIZE>::apply2(Matrix<T>& matrix)
{
    auto& e = matrix.elems;
    std::transform(&this->storage[0], &this->storage[0] + this->_size, &this->storage[0], [&e](gpu_T& a) {
        gpu_T value{ (e[0] * a[0] + e[1] * a[1] + e[2] * 1), (e[3] * a[0] + e[4] * a[1] + e[5] * 1) };
        return value;
    });
}

/* Col Major 2D*/
template<typename T>
Matrix<T>::Matrix(T x1, T y1, T z1, T x2, T y2, T z2, T x3, T y3, T z3)
{
    elems[0] = x1;
    elems[1] = y1;
    elems[2] = z1;
    elems[3] = x2;
    elems[4] = y2;
    elems[5] = z2;
    elems[6] = x3;
    elems[7] = y3;
    elems[8] = z3;
}

template<typename T>
std::array<T, 9>* Matrix<T>::data()
{
    return &elems;
}

template<typename T>
Matrix<T> Matrix<T>::identity()
{
    auto m = Matrix<T>{ 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    return m;
}

template<typename T>
T Matrix<T>::det()
{
    auto& e = elems;
    auto det = e[0] * e[4] * e[8] + e[3] * e[7] * e[2] + e[6] * e[1] * e[5] - e[0] * e[7] * e[2] - e[3] * e[1] * e[8]
               - e[6] * e[4] * e[2];
    return det;
}

template<typename T>
bool Matrix<T>::isSingular()
{
#if DEVELOPMENT
    return (det() == 0);
#else
    return false;
#endif
}

template<typename T>
Matrix<T> Matrix<T>::scale(T x, T y)
{
    auto m = Matrix<T>{ x, 0, 0, 0, y, 0, 0, 0, 1 };
    return m;
}

template<typename T>
Matrix<T> Matrix<T>::rotate(T deg)
{
    T rad = _degToRad(deg);
    auto m = Matrix<T>{ cos(rad), sin(rad), 0, -sin(rad), cos(rad), 0, 0, 0, 1 };
    return m;
}

template<typename T>
Matrix<T> Matrix<T>::translate(T x, T y)
{
    auto m = Matrix<T>{ 1, 0, 0, 0, 1, 0, x, y, 1 };
    return m;
}

template<typename T>
double Matrix<T>::_radToDeg(double rad)
{
    return rad * (180.0 / M_PI);
}
template<typename T>
double Matrix<T>::_degToRad(double deg)
{
    return deg / (180.0 / M_PI);
}

template<typename T>
Matrix<T> Matrix<T>::mul(const Matrix<T>& a, const Matrix<T>& b)
{
    auto m = Matrix<T>{
        a.elems[0] * b.elems[0] + a.elems[3] * b.elems[1] + a.elems[6] * b.elems[2], // c00
        a.elems[0] * b.elems[3] + a.elems[3] * b.elems[4] + a.elems[6] * b.elems[5], // c01
        a.elems[0] * b.elems[6] + a.elems[3] * b.elems[7] + a.elems[6] * b.elems[8], // c02
        a.elems[1] * b.elems[0] + a.elems[4] * b.elems[1] + a.elems[7] * b.elems[2], // c10
        a.elems[1] * b.elems[3] + a.elems[4] * b.elems[4] + a.elems[7] * b.elems[5], // c11
        a.elems[1] * b.elems[6] + a.elems[4] * b.elems[7] + a.elems[7] * b.elems[8], // c12
        a.elems[2] * b.elems[0] + a.elems[5] * b.elems[1] + a.elems[8] * b.elems[2], // c20
        a.elems[2] * b.elems[3] + a.elems[5] * b.elems[4] + a.elems[8] * b.elems[5], // c21
        a.elems[2] * b.elems[6] + a.elems[5] * b.elems[7] + a.elems[8] * b.elems[8]  // c22
    };
    return m;
}

template<typename T>
Stack<T>::Stack()
    : std::vector<Matrix<T>>(){};

template<typename T>
void Stack<T>::identity()
{
    this->clear();
}

template<typename T>
void Stack<T>::scale(T x, T y)
{
    if (!((1 == x) && (1 == y)))
        this->push_back(Matrix<T>::scale(x, y));
}

template<typename T>
void Stack<T>::rotate(T deg)
{
    if (!(deg == 0))
        this->push_back(Matrix<T>::rotate(deg));
}

template<typename T>
void Stack<T>::translate(T x, T y)
{
    if (!((0 == x) && (0 == y)))
        this->push_back(Matrix<T>::translate(x, y));
}

template<typename T>
struct Vector
{
    Vector(std::initializer_list<T> list)
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
Vector<T> operator+(const Vector<T>& a, const Vector<T>& b)
{
    auto c = Vector<T>{};
    c.vec[0] = a.vec[0] + b.vec[0];
    c.vec[1] = a.vec[1] + b.vec[1];
    return c;
};
template<typename T>
std::ostream& operator<<(std::ostream& os, const Vector<T>& v)
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
    using C1 = Vectors<Vector<T>>;
    auto st = Stack<T>();
    st.identity();
    // st.scale(2, 2);

    auto sq = C1(5);
    T square = 0.5;
    sq[0] = Vector<T>{ square, square };
    sq[1] = Vector<T>{ -square, square };
    sq[2] = Vector<T>{ -square, -square };
    sq[3] = Vector<T>{ square, -square };
    sq[4] = Vector<T>{ square, square };
    sq.apply(st);
    // drawVectors(sq);
    // std::for_each(std::begin(sq), std::end(sq), [](auto& v) { std::cout << v << ","; });

    auto v1 = Vector<T>{ 1, 0 };
    auto v2 = Vector<T>{ sqrt(2) / 2, sqrt(2) / 2 };
    auto v3 = Vector<T>{ 0, 1 };
    auto _data_sink = C1(3);
    _data_sink[0] = v1;
    _data_sink[1] = v1 + v2;
    _data_sink[2] = v1 + v2 + v3;

    // auto last_pair = _data_sink.back();//multiple tracks
    auto last_pair = _data_sink;
    auto left = last_pair;

    auto middle1 = _data_sink[0];
    auto middle2 = _data_sink[1];
    double angle2 = Matrix<T>::_radToDeg(atan2((middle2[0] - middle1[1]), (middle2[0] - middle1[1])));
    // glRotatef(-angle2, 0.0f, 0.0f, 1.0f);
    st.rotate(-angle2);
    st.translate(-middle1[0], -middle1[1]);

    // middle marker
    auto mid = C1(2);
    mid[0] = Vector<T>{ middle1[0], middle1[1] };
    mid[1] = Vector<T>{ middle2[0], middle2[1] };
    mid.apply(st);
    // drawVectors(mid);
    // std::for_each(std::begin(mid), std::end(mid), [](auto& v) { std::cout << v << ","; });

    auto vecs = C1(left.size() + 1);
    vecs[0] = Vector<T>{ 0, 0 };
    for (int i = 1; i <= vecs.size(); i++)
    {
        vecs[i] = Vector<T>{ left[i - 1][0], left[i - 1][1] };
    }
    vecs.apply(st);
    // drawVectors(vecs);
    std::for_each(&vecs[0], &vecs[0] + vecs.size(), [](auto& v) { std::cout << v << ","; });

    return 0;
}
#include <array>
#include <iterator>
#include <vector>

template<typename T, size_t Size = 2, typename tf = typename std::enable_if_t<std::is_arithmetic<T>::value>>
class Vector
{
public:
    using element = typename std::array<T, Size>;
    typedef typename element::value_type value_type;
    typedef typename element::reference reference;

    Vector(std::initializer_list<T> list);
    Vector<T, Size, tf>& __init(std::initializer_list<T> list); // REMOVE: not a pod

    std::array<T, Size> coords;
};

// template<typename Container, typename tf = typename std::enable_if_t<std::is_pod<typename
// Container::value_type>::value>> class Vectors;//forward declaration! same as in real declaration below
// template<typename Container, typename tf = typename std::enable_if_t<std::is_trivially_constructible<typename
// Container::value_type>::value>> class Vectors;//forward declaration! same as in real declaration below
template<typename Container, typename tf = typename std::enable_if_t<true>>
class Vectors; // forward declaration! same as in real declaration below

/* Row Major 2D*/
template<typename Container>
class Matrix
{
public:
    using PodClass = typename Container::value_type;
    using T = typename Container::value_type::value_type;

    friend class Matrix;
    friend class Vectors<Container>;

    Matrix(const Matrix& other) = default;
    Matrix(Matrix&& other) = default;
    explicit Matrix(T x1, T x2, T x3, T y1, T y2, T y3, T z1, T z2, T z3);
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

    // MSVC: function return type (and signature) is read before alias definition
    typename Container::value_type::value_type det();

    bool isSingular();

    static Matrix scale(T x, T y);

    static Matrix rotate(T deg);

    static Matrix translate(T x, T y);
    double static _radToDeg(double rad);
    double static _degToRad(double deg);

private:
    static Matrix mul(const Matrix a, const Matrix b);
    void apply3(Vectors<Container>& transform);
    void apply2(Vectors<Container>& transform);

private:
    std::array<T, 9> elems;
};

// todo disable type conversions here?
template<typename Container>
class Stack : public std::vector<Matrix<Container>>
{

public:
    using PodClass = typename Container::value_type;
    using T = typename PodClass::value_type;

    Stack();

    void identity();
    void scale(T x, T y);
    // this was void rotate(FLOAT deg); why?!
    void rotate(T deg);
    void translate(T x, T y);
};

template<typename Container,
         typename // = typename std::enable_if_t<std::is_pod<PodClass>::value>
         >        // same as in forward declaration above!
class Vectors : public Container
{
    // todo
    // we cannot make Vectors a pod type
    // AND std::vector can not use memory of numpy_array; (they're both in stack)
    //-> no move constructor from numpy array
public:
    using Container::Container;
    Vectors();
    Vectors(const Container& other);
    Vectors(Container&& other);
    Vectors& operator=(Container&& other);

    using PodClass = typename Container::value_type;
    using T = typename PodClass::value_type;

    void apply(Stack<Container>& stack);

    /*const T* _to_C_array(){
        if (this->size()>0)
            return &(this->at(0))[0];
        else
            return nullptr;
    }*/
};

#include <cmath>
#include <algorithm>
#include <stdexcept>

// not available on windows:
#ifndef M_PI
#define M_PI        3.14159265358979323846264338327950288
#endif

template<typename T, size_t Size, typename tf> Vector<T, Size, tf>::Vector(std::initializer_list<T> list) {
	__init(list);
}

template<typename T, size_t Size, typename tf> Vector<T, Size,tf>& Vector<T,Size,tf>::__init(std::initializer_list<T> list) {
	std::copy(std::begin(list),std::end(list),std::begin(coords));
	std::fill(std::begin(coords)+list.size(), std::end(coords), T(0));
	return *this;
}

template<typename Container, typename tf> Vectors<Container,tf>::Vectors() : Container() {
}

template<typename Container, typename tf> Vectors<Container,tf>::Vectors(const Container& other) : Container(other) {
//	int test = 5/0;
//	*this = Vectors<Container,tf>();//other;
}

template<typename Container, typename tf> Vectors<Container,tf>::Vectors(Container&& other) : Container{std::move(other)} {
	//Vectors<Container,tf> temp = std::move(other);
	//*this = temp;
	//*this = static_cast<Container>(other);
	//*this = std::move(other);
}

template<typename Container, typename tf> Vectors<Container,tf>& Vectors<Container,tf>::operator=(Container&& other) {
	//Vectors<Container,tf> temp = std::move(other);
	Container::operator=(std::move(other));
	return *this;
}

//propagating: par
template<typename Container, typename tf>void Vectors<Container,tf>::apply(Stack<Container>& stack) {
	auto& me = *this;
	auto first = std::find_if(std::begin(stack), std::end(stack), [](Matrix<Container>& m){ return !m.isSingular();});
	if (first!=std::end(stack)){
#if DEVELOPMENT
		if (*first==Matrix<Container>::identity())
			throw std::logic_error("multiplying identity matrix has performance penalty. Check before adding to stack.");
#endif
		Matrix<Container> all = *first;
		std::for_each(++first, std::end(stack), [&all](Matrix<Container>& m) {
#if DEVELOPMENT
			if (m==Matrix<Container>::identity())
				throw std::logic_error("multiplying identity matrix has performance penalty. Check before adding to stack.");
#endif
			if (!m.isSingular())
				all = Matrix<Container>::mul(all,m);
		});
		static const size_t size = PodClass{}.coords.size();
		if (size < 2)
			throw std::logic_error("Matrix class does not work with vector dimensions lower than 2");
		else if (size == 2)
			all.apply2(me);
		else
			all.apply3(me);
	}
	/*std::for_each(std::rbegin(stack), std::rend(stack), [&me](Matrix<Container>& m) {
		m.apply(me);
	});*/
}

/* Row Major 2D*/
template<typename Container> Matrix<Container>::Matrix(T x1, T x2, T x3, T y1, T y2, T y3, T z1, T z2, T z3) {
	elems[0] = x1;
	elems[1] = x2;
	elems[2] = x3;
	elems[3] = y1;
	elems[4] = y2;
	elems[5] = y3;
	elems[6] = z1;
	elems[7] = z2;
	elems[8] = z3;
}

template<typename Container> std::array<typename Container::value_type::value_type,9>* Matrix<Container>::data(){
	return &elems;
}

template<typename Container> Matrix<Container> Matrix<Container>::identity() {
	auto m = Matrix<Container>{	T(1),T(0),T(0),
		T(0),T(1),T(0),
		T(0),T(0),T(1)
	};
	return std::move(m);
}

template<typename Container> typename Container::value_type::value_type Matrix<Container>::det() {
	auto& e = elems;
	auto det = e[0]*e[4]*e[8]+e[1]*e[5]*e[6]+e[2]*e[3]*e[7]-e[0]*e[5]*e[6]-e[1]*e[3]*e[8]-e[2]*e[4]*e[6];
	return det;
}

template<typename Container> bool Matrix<Container>::isSingular(){
#if DEVELOPMENT
	return (det()==0);
#else
	return false;
#endif
}

template<typename Container> Matrix<Container> Matrix<Container>::scale(T x, T y) {
	auto m = Matrix<Container>{	T(x),T(0),T(0),
		T(0),T(y),T(0),
		T(0),T(0),T(1)
	};
	return std::move(m);
}

template<typename Container> Matrix<Container> Matrix<Container>::rotate(T deg) {
	T rad = _degToRad(deg);
	auto m = Matrix<Container>{	T(cos(rad)),T(-sin(rad)),T(0),
						T(sin(rad)),T(cos(rad)),T(0),
						T(0),T(0),T(1)
	};
	return std::move(m);
}

template<typename Container> Matrix<Container> Matrix<Container>::translate(T x, T y) {
	auto m = Matrix<Container>{	T(1),T(0),T(x),
		T(0),T(1),T(y),
		T(0),T(0),T(1)
	};
	return std::move(m);
}

template<typename Container>double Matrix<Container>::_radToDeg(double rad) { return rad * (180.0 / M_PI); }//  pi/rad = 180/x, x(pi/rad)=180, x=180/(pi/rad)
template<typename Container>double Matrix<Container>::_degToRad(double deg) { return deg / (180.0 / M_PI); }

template<typename Container> Matrix<Container> Matrix<Container>::mul(const Matrix<Container> a, const Matrix<Container> b) {
	auto m = Matrix<Container>{	a.elems[0] * b.elems[0]+a.elems[1] * b.elems[3]+a.elems[2] * b.elems[6],	//c00
						a.elems[0] * b.elems[1]+a.elems[1] * b.elems[4]+a.elems[2] * b.elems[7],	//c01
						a.elems[0] * b.elems[2]+a.elems[1] * b.elems[5]+a.elems[2] * b.elems[8],	//c02
						a.elems[3] * b.elems[0]+a.elems[4] * b.elems[3]+a.elems[5] * b.elems[6],	//c10
						a.elems[3] * b.elems[1]+a.elems[4] * b.elems[4]+a.elems[5] * b.elems[7],	//c11
						a.elems[3] * b.elems[2]+a.elems[4] * b.elems[5]+a.elems[5] * b.elems[8],	//c12
						a.elems[6] * b.elems[0]+a.elems[7] * b.elems[3]+a.elems[8] * b.elems[6],	//c20
						a.elems[6] * b.elems[1]+a.elems[7] * b.elems[4]+a.elems[8] * b.elems[7],	//c21
						a.elems[6] * b.elems[2]+a.elems[7] * b.elems[5]+a.elems[8] * b.elems[8]		//c22
	};
	return std::move(m);
}

//this may have to be compiled with a different compiler: NOT HEADER ONLY
//readonly: par_unseq
template<typename Container> void Matrix<Container>::apply3(Vectors<Container>& transform) {
	auto& e = elems;
	std::transform(std::begin(transform), std::end(transform), std::begin(transform), [&e](PodClass a) {
		PodClass value{
		(e[0] * a.coords[0] + e[1] * a.coords[1] + e[2] * a.coords[2]),
		(e[3] * a.coords[0] + e[4] * a.coords[1] + e[5] * a.coords[2]),
		(e[6] * a.coords[0] + e[7] * a.coords[1] + e[8] * a.coords[2])
		};
		return value;
		/*return PodClass{}.__init({
			(e[0]* a.coords[0] +e[1]* a.coords[1] +e[2]* a.coords[2]),
			(e[3]* a.coords[0] +e[4]* a.coords[1] +e[5]* a.coords[2]),
			(e[6]* a.coords[0] +e[7]* a.coords[1] +e[8]* a.coords[2])}
		);*/
	});
}
template<typename Container> void Matrix<Container>::apply2(Vectors<Container>& transform) {
	auto& e = elems;
	std::transform(std::begin(transform), std::end(transform), std::begin(transform), [&e](PodClass a) {
		PodClass value{
		(e[0] * a.coords[0] + e[1] * a.coords[1] + e[2] * 1),
		(e[3] * a.coords[0] + e[4] * a.coords[1] + e[5] * 1)
		};
		return value;
		/*return PodClass{}.__init({
			(e[0] * a.coords[0] + e[1] * a.coords[1] + e[2] * 1),
			(e[3] * a.coords[0] + e[4] * a.coords[1] + e[5] * 1)}
		);*/
	});
}

template<typename Container> Stack<Container>::Stack() : std::vector<Matrix<Container>>() {};

template<typename Container> void Stack<Container>::identity(){
	this->clear();
}

template<typename Container> void Stack<Container>::scale(T x,T y){
	if (!(( T(1.0)==x)&&( T(1.0)==y)))
		this->push_back( Matrix<Container>::scale(x,y));
}

template<typename Container> void Stack<Container>::rotate(T deg){
	if (!(deg== T(0.0) ))
		this->push_back( Matrix<Container>::rotate(deg));
}

template<typename Container> void Stack<Container>::translate(T x,T y){
	if (!(( T(0.0)==x)&&( T(0.0)==y)))
		this->push_back( Matrix<Container>::translate(x, y));
}

int main()
{
    return 0;
}
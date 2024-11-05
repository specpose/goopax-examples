#pragma once

#include <goopax>

namespace std
{

template<typename T>
ostream& operator<<(ostream& s, const vector<T>& v);
template<typename S, typename T, size_t N>
S& operator<<(S& s, const array<T, N>& v);

template<typename V, typename S>
S& output_vec(S& s, const V& v)
{
    s << "(";
    bool first = true;
    for (auto p = v.begin(); p != v.end(); ++p)
    {
        if (!first)
            s << ",";
        first = false;
        if constexpr (sizeof(typename goopax::unrangetype<V>::type) == 1)
        {
            s << (int)(*p);
        }
        else
        {
            s << *p;
        }
    }
    s << ")";
    return s;
}

template<typename T>
ostream& operator<<(ostream& s, const vector<T>& v)
{
    return output_vec(s, v);
}
template<typename S, typename T, size_t N>
S& operator<<(S& s, const array<T, N>& v)
{
    return output_vec(s, v);
}

template<typename T, typename size_type>
ostream& operator<<(ostream& s, const goopax::buffer<T, size_type>& b)
{
    return s << b.to_vector();
}

}

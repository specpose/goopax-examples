#include <Python.h>
#include <goopax>
typedef double T; // FIX: compilation as module

/*  Goopax  */
bool test_dev(unsigned int env) {
    goopax::goopax_device d = goopax::default_device(static_cast<goopax::envmode>(env));
    if (d.valid())
        return true;
    unsigned int log_env = env >> int(std::log2(env));
    //throw std::runtime_error("Default Device in ENV "+std::to_string(env)+" ("+backend_names[log_env]+")"+" not valid.");
    return false;
}

/*  Logic   */
std::array<Py_ssize_t, PyBUF_MAX_NDIM> make_strides(const std::array<Py_ssize_t, PyBUF_MAX_NDIM>& shape, const Py_ssize_t itemsize) {
    auto strides = std::array<Py_ssize_t, PyBUF_MAX_NDIM>{};
    std::copy(begin(shape), end(shape), begin(strides));
    Py_ssize_t previous_stridesize = itemsize;
    std::for_each(std::rbegin(strides), std::rend(strides), [&previous_stridesize](auto& current){
        if (current != 0) {
            auto shape = current;
            current = previous_stridesize;
            previous_stridesize = shape * previous_stridesize;
        }
    });
    return strides;
};

/*  Python  */
Py_ssize_t itemsize(std::string format){
    PyObject* struct_m = PyImport_ImportModule("struct");
    PyObject* calcsize = PyObject_GetAttrString(struct_m,"calcsize");
    PyObject* calcsize_args = PyTuple_New(1);
    PyTuple_SetItem(calcsize_args, 0, PyUnicode_FromString(format.c_str()));
    PyObject* itemsize = (PyObject*)PyObject_CallObject(calcsize, calcsize_args);
    if (PyNumber_Check(itemsize)!=1)
        abort();
    return PyNumber_AsSsize_t(itemsize,NULL);
}
Py_ssize_t ndim(Py_ssize_t * shape) {
    Py_ssize_t ndim = 0;
    for (std::size_t i = 0; i <PyBUF_MAX_NDIM; ++i)
        if (!shape[i]==0)
            ++ndim;
        else
            break;
    return ndim;
};
Py_ssize_t len(Py_ssize_t itemsize, Py_ssize_t * shape) {
    auto len = itemsize;
    for (Py_ssize_t i = 0; i<ndim(shape); ++i)
        len *= shape[i];
    return len;
}
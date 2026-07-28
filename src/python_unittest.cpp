#include <stdio.h>
#include <goopax_extra/types.hpp>

//#define PY_SSIZE_T_CLEAN
//#define Py_LIMITED_API 0x030C0000
#include "gpu_buffer.cpp"

typedef double T;
static const T test_data[] = {0,0,1,0,1+sqrt(2)/2,sqrt(2)/2,1+sqrt(2)/2,1+sqrt(2)/2};
static const char _format[2] = "d"; // T
static const std::size_t _alignment = 4;
static const std::array<std::size_t, 2> _shape = {4,2};
static const Py_ssize_t expected_vec_dims = 2;

#ifndef NDEBUG
#define Py_DECREF_bak Py_DECREF
void __delete(PyObject* X){
    //int* refcnt = (int*)(X);
    //*refcnt = 0;
    Py_DECREF_bak(X);
    if (Py_REFCNT(X) != 0)
        printf("%d\n",Py_REFCNT(X));
    if (Py_REFCNT(X) < 0)
        abort();
}
#undef Py_DECREF
#define Py_DECREF(X) __delete(X);
#endif

PyObject* _construct_CustomBuffer(decltype(_format), decltype(_shape) shape, decltype(_alignment) alignment){
    PyObject* gpu_buffer = PyImport_ImportModule("gpu_buffer");
    PyObject* custom_buffer = PyObject_GetAttrString(gpu_buffer,"CustomBuffer");
    PyObject* custom_buffer_args = PyTuple_New(3);
    PyTuple_SetItem(custom_buffer_args, 0, PyUnicode_FromString(_format));
    PyObject* s = PyTuple_New(std::size(shape));
    for(std::size_t i=0; i<std::size(shape); ++i)
        PyTuple_SetItem(s, i, PyLong_FromLong(shape.at(i)));
    PyTuple_SetItem(custom_buffer_args, 1, s);
    PyTuple_SetItem(custom_buffer_args, 2, PyLong_FromLong(alignment));
    PyObject* myobj = PyObject_CallObject(custom_buffer, custom_buffer_args);  // .tp_new
    //CustomBuffer* myobj = PyObject_New(CustomBuffer,MyType);
    //PyObject* myobj_ = PyObject_Init((PyObject*)myobj, MyType);
    return myobj;
}

static void process(void* buffer, Py_ssize_t* strides, Py_ssize_t* shape){
    printf("shape=(%d,%d)\n",shape[0],shape[1]);
    T* _buffer = (T*)buffer;
    for (Py_ssize_t i=0; i<shape[0]*shape[1]; i+=2)
        printf("(%f,%f)\n",_buffer[i],_buffer[i+1]);
}

#ifdef __cplusplus
#include <boost/python/make_function.hpp>
#include <boost/python/module.hpp>
#include <boost/python/numpy.hpp>
#include <boost/python/ptr.hpp>

namespace py = boost::python;
namespace np = boost::python::numpy;

static void pynumpy_process_ndarray(np::ndarray& array) {
    Py_ssize_t* c_strides = const_cast<Py_ssize_t*>(array.get_strides());
    Py_ssize_t* c_shape = const_cast<Py_ssize_t*>(array.get_shape());
    process(array.get_data(), c_strides, c_shape);
}

py::object pynumpy_test_ndarray() {
    printf("begin: pynumpy_test_ndarray\n");
    PyObject* buffy = _construct_CustomBuffer(_format, _shape, _alignment);
    Py_buffer* view = new Py_buffer;
    if (PyObject_GetBuffer(buffy,view, PyBUF_C_CONTIGUOUS | PyBUF_FORMAT)==0){
    //py::tuple nd_shape = py::make_tuple(sizeof(test_data)/sizeof(T));
    np::dtype dtype = np::dtype::get_builtin<T>();
    //np::ndarray tmp = np::zeros(nd_shape, dtype);
    np::ndarray tmp = np::from_data(view->buf, dtype,py::make_tuple(sizeof(test_data)/sizeof(T)), py::make_tuple(sizeof(double)),py::object());
    for (std::size_t i = 0; i < sizeof(test_data)/sizeof(T); i++)
        tmp[i] = test_data[i];
    tmp = tmp.reshape(py::make_tuple(sizeof(test_data)/sizeof(T)/2,2));
    py::object pynumpy = py::import("pynumpy");
    pynumpy.attr("process_ndarray")(tmp);

    PyBuffer_Release(view);
    delete view;
    }
    PyObject_Del((void*)buffy);
    printf("end: pynumpy_test_ndarray\n");
    return py::long_(0);
}

BOOST_PYTHON_MODULE(pynumpy)
{
    np::initialize();
    py::def("process_ndarray", py::make_function(pynumpy_process_ndarray));
    py::def("test_ndarray", pynumpy_test_ndarray, py::return_value_policy<py::return_by_value>());
}
#endif

static PyObject* pylist_process_list(PyObject* self, PyObject* args){
    PyObject* list = NULL;
    PyArg_ParseTuple(args, "O", &list);
    Py_ssize_t shape_1 = PyList_Size(list);
    T* buffer;
    buffer = (T*)malloc(expected_vec_dims*sizeof(T)*shape_1);
    for(Py_ssize_t i=0; i<shape_1; i++){
        PyObject* item = PyList_GetItem(list,i);
        //printf("%x: Retrieving vec2d from list\n",item);
        Py_ssize_t shape_2 = PyTuple_Size(item);
        if (shape_2 != expected_vec_dims)
            abort();
        for (Py_ssize_t j=0; j<shape_2; j++){
            PyObject* e = PyTuple_GetItem(item,j);
            buffer[shape_2*i+j]=PyFloat_AsDouble(e);
            //printf("%f,\n",buffer[shape_2*i+j]);//T
        }
        Py_DECREF(item);//1
    }
    Py_ssize_t* shape = (Py_ssize_t*)malloc(2*sizeof(Py_ssize_t));
    shape[0]=shape_1;
    shape[1]=expected_vec_dims;
    Py_ssize_t* strides = (Py_ssize_t*)malloc(2*sizeof(Py_ssize_t));
    strides[0]=expected_vec_dims*sizeof(T);
    strides[1]=sizeof(T);
    process((void*)buffer,strides,shape);
    free(shape);
    free(strides);
    free(buffer);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* _vec2d(T x, T y){
    PyObject* result = PyTuple_New(expected_vec_dims);
    //printf("%x: New Vec2d created\n",result);
    PyObject* x_ = PyFloat_FromDouble(x);//T
    PyObject* y_ = PyFloat_FromDouble(y);//T
    PyTuple_SetItem(result, 0, x_);
    PyTuple_SetItem(result, 1, y_);
    return result;
}

static PyObject* pylist_process_memoryview(PyObject* self, PyObject* args){
    PyObject* mvobject = NULL;
    PyArg_ParseTuple(args, "O", &mvobject);
    if (PyMemoryView_Check(mvobject)!=1)
        abort();
    Py_buffer* view = PyMemoryView_GET_BUFFER(mvobject);
    process(view->buf,view->strides,view->shape);
    Py_INCREF(Py_None);
    return Py_None;
}

#if !MODULE_LIBRARY
static PyObject* pylist_test_list(PyObject* self, PyObject* what){
    printf("begin: pylist_test_list\n");
    PyObject* list = PyList_New(0);
    for (size_t i=0;i<sizeof(test_data)/sizeof(T);i+=2){
        PyObject* item = _vec2d(test_data[i],test_data[i+1]);
        PyList_Append(list, item);
    }
    PyObject* printList = PyObject_GetAttrString(self,"process_list");
    PyObject* args = PyTuple_New(1);
    PyTuple_SetItem(args, 0, list);
    PyObject* result = PyObject_CallObject(printList, args);
    Py_DECREF(printList);
    printf("end: pylist_test_list\n");
    return PyLong_FromLong(0);
}

static PyObject* pylist_test_memoryview(PyObject* self, PyObject* what){
    printf("begin: pylist_test_memoryview\n");
    PyObject* myobj = _construct_CustomBuffer(_format, _shape, _alignment);
    Py_buffer view;
    if (PyObject_GetBuffer((PyObject*)myobj,&view, PyBUF_C_CONTIGUOUS | PyBUF_FORMAT)==0){
        PyObject* mvobject = PyMemoryView_FromBuffer(&view);
        PyObject* printMemoryview = PyObject_GetAttrString(self,"process_memoryview");
        PyObject* args = PyTuple_New(1);
        PyTuple_SetItem(args, 0, mvobject);
        PyObject* result = PyObject_CallObject(printMemoryview, args);
        PyBuffer_Release(&view);
    }
    printf("test: repeated GetBuffer\n");
    if (PyObject_GetBuffer((PyObject*)myobj,&view, PyBUF_C_CONTIGUOUS | PyBUF_FORMAT)==0){
        PyBuffer_Release(&view);
    }
    PyObject_Del((void*)myobj);
    printf("end: pylist_test_memoryview\n");
    return PyLong_FromLong(0);
}
#endif

static PyMethodDef pylist_methods[] = {
    {"process_list", pylist_process_list, METH_VARARGS, ""},
    {"process_memoryview", pylist_process_memoryview, METH_VARARGS, ""},
#if !MODULE_LIBRARY
    {"test_list", pylist_test_list, METH_NOARGS, ""},
    {"test_memoryview", pylist_test_memoryview, METH_NOARGS, ""},
#endif
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pylist_module = {
    PyModuleDef_HEAD_INIT,
    "pylist",
    "",
    0,
    pylist_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC PyInit_pylist(void){
    return PyModule_Create(&pylist_module);
}

#if !MODULE_LIBRARY
int main(){
    PyImport_AppendInittab("gpu_buffer", PyInit_gpu_buffer);
#ifdef __cplusplus
    PyImport_AppendInittab("pynumpy", PyInit_pynumpy);
#endif
    PyImport_AppendInittab("pylist", PyInit_pylist);
#ifndef NDEBUG
    PyInitConfig *config = PyInitConfig_Create();
    PyInitConfig_SetInt(config, "dev_mode", 1);
    Py_InitializeFromInitConfig(config);
    PyInitConfig_Free(config);
#else
    Py_Initialize();
#endif
    PyGILState_STATE gstate = PyGILState_Ensure();

    PyObject* unittest = PyImport_ImportModule("unittest");
    PyObject* tcase = PyObject_GetAttrString(unittest,"FunctionTestCase");
#ifdef __cplusplus
    PyObject* pynumpy = PyImport_ImportModule("pynumpy");
    //PyObject* tn = PyObject_CallMethodNoArgs(pynumpy,PyUnicode_FromString("test_ndarray"));
    PyObject* tn = PyObject_GetAttrString(pynumpy,"test_ndarray");
    PyObject* case3_args = PyTuple_New(1);
    PyTuple_SetItem(case3_args, 0, tn);
    PyObject* case3 = PyObject_CallObject(tcase,case3_args);
#endif
    PyObject* pylist = PyImport_ImportModule("pylist");
    //PyObject* tl = PyObject_CallMethodNoArgs(pylist,PyUnicode_FromString("test_list"));
    PyObject* tl = PyObject_GetAttrString(pylist,"test_list");
    //PyObject* tm = PyObject_CallMethodNoArgs(pylist,PyUnicode_FromString("test_memoryview"));
    PyObject* tm = PyObject_GetAttrString(pylist,"test_memoryview");
    PyObject* tsuite = PyObject_GetAttrString(unittest,"TestSuite");
    PyObject* tresult = PyObject_GetAttrString(unittest,"TestResult");
    if (!tresult)
        printf("TestResult class was not found in unittest\n");
    PyObject* case1_args = PyTuple_New(1);
    PyTuple_SetItem(case1_args, 0, tl);
    PyObject* case1 = PyObject_CallObject(tcase,case1_args);
    PyObject* case2_args = PyTuple_New(1);
    PyTuple_SetItem(case2_args, 0, tm);
    PyObject* case2 = PyObject_CallObject(tcase,case2_args);
    PyObject* tsuite_args = PyTuple_New(1);
#ifdef __cplusplus
    PyObject* tests_tuple = PyTuple_New(3);
    PyTuple_SetItem(tests_tuple,2,case3);
#else
    PyObject* tests_tuple = PyTuple_New(2);
#endif
    PyTuple_SetItem(tests_tuple,0,case1);
    PyTuple_SetItem(tests_tuple,1,case2);
    PyTuple_SetItem(tsuite_args,0,tests_tuple);
    PyObject* runner = PyObject_CallObject(tsuite, tsuite_args);
    PyObject* run_method = PyObject_GetAttrString(runner,"run");
    if (!run_method)
        printf("run_method not found\n");
    PyObject* result = PyObject_CallObject(tresult,NULL);
    if (!result)
        printf("Result was not instantiated\n");
    PyObject* run_args = PyTuple_New(1);
    PyTuple_SetItem(run_args,0,result);
    PyObject* run = PyObject_CallObject(run_method,run_args);
    if (!run)
        abort();
    PyObject* times = PyObject_GetAttrString(result,"collectedDurations");
    for (int i=0;i<PyList_Size(times);i++) {
        PyObject* tindex = PyList_GetItem(times,i);
        printf("%s took %f.\n",PyTuple_GetItem(tindex,0),PyFloat_AsDouble(PyTuple_GetItem(tindex,1)));
    }
    int uS = PyList_Size(PyObject_GetAttrString(result,"unexpectedSuccesses"));
    int f = PyList_Size(PyObject_GetAttrString(result,"failures"));
    int e = PyList_Size(PyObject_GetAttrString(result,"errors"));
    long tR = PyLong_AsLong(PyObject_GetAttrString(result,"testsRun"));

    PyGILState_Release(gstate);
    int py_finalized = Py_FinalizeEx();
    return tR>0&&e==0&&f==0&&uS==0&&py_finalized ? 0 : py_finalized;
}
#endif

#ifndef NDEBUG
#undef Py_DECREF
#define Py_DECREF Py_DECREF_bak;
#endif
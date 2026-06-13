#include <goopax>
#include <goopax_extra/types.hpp>
#include <Python.h>
#define PY_SSIZE_T_CLEAN
#define Py_LIMITED_API
static Py_ssize_t queue_variables = 3;
static Py_ssize_t user_variables = 1;
#include "gpu_buffer.hpp"
typedef double T;
typedef struct {
    PyObject_VAR_HEAD
    Py_ssize_t* padding;
    Py_ssize_t* shape;
    Py_ssize_t* strides;
    char* format;
    T* ptr;
} CustomBuffer;
int get_buffer(PyObject *exporter, Py_buffer *view, int flags){ // TODO: internal struct exposed to consumer
    printf("GetBuffer\n");
    view->readonly = 0;
    view->suboffsets = NULL;
    T* ptr = ((CustomBuffer*)exporter)->ptr;
    view->buf = (void*)ptr;
    view->itemsize = itemsize(((CustomBuffer*)exporter)->format);
    Py_ssize_t* _shape = ((CustomBuffer*)exporter)->shape;
    if ((flags & PyBUF_FORMAT) == PyBUF_FORMAT) {
        view->format = ((CustomBuffer*)exporter)->format;
    } else {
        view->format = NULL;
    }
    if ((flags & PyBUF_STRIDES) == PyBUF_STRIDES) {
        printf("PyBUF_STRIDES\n");
        view->strides = ((CustomBuffer*)exporter)->strides;
    } else {
        view->strides = NULL;
    }
    if ((flags & PyBUF_ND) == PyBUF_ND) {
        printf("PyBUF_ND\n");
        view->shape = _shape;
        view->ndim = ndim(_shape);
        view->len = len(view->itemsize, _shape);
        view->obj = exporter;
        return 0;
    } else if ((flags & PyBUF_SIMPLE) == PyBUF_SIMPLE) {
        printf("PyBUF_SIMPLE\n");
        view->shape = NULL;
        view->ndim = 1;
        view->len = len(view->itemsize, _shape);
        view->obj = exporter;
        return 0;
    }
    view->obj = NULL;
    return -1;
};
void release_buffer(PyObject *exporter, Py_buffer *view){
    printf("ReleaseBuffer\n");
};
PyObject *CustomBuffer_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds){
    printf("New\n");
    CustomBuffer* buffy = (CustomBuffer*)subtype->tp_alloc(subtype, 0);
    if (buffy != NULL) {
        buffy->padding = (Py_ssize_t*)malloc(sizeof(Py_ssize_t));
        buffy->shape = (Py_ssize_t*)malloc(PyBUF_MAX_NDIM*sizeof(Py_ssize_t));
        int alignment = 4;
        const char* format = NULL;
        Py_ssize_t format_length = 0;
        PyObject* shape = NULL;
        if (PyArg_ParseTuple(args, "s#Oi", &format, &format_length, &shape, &alignment)) {
            buffy->format = (char*)malloc(format_length);
            strcpy(buffy->format, format);
            const auto _itemsize = itemsize(format);
            if (PyTuple_Check(shape) != 1)
                abort();
            const auto ndim = PyTuple_Size(shape);
            for (size_t i=0; i<PyBUF_MAX_NDIM; i++)
                buffy->shape[i] = 0;
            /*  Hack: Needs parse for buffer allocation   */
            Py_ssize_t shape_prod = 1;
            for (Py_ssize_t i = 0; i<ndim; ++i) {
                if (PyNumber_Check(PyTuple_GetItem(shape, i)) != 1)
                    abort;
                const auto value = PyNumber_AsSsize_t(PyTuple_GetItem(shape, i), NULL);
                shape_prod *= value;
                buffy->shape[i] = value;
            }
            buffy->strides = (Py_ssize_t*)malloc(PyBUF_MAX_NDIM*sizeof(Py_ssize_t));
            auto _strides = make_strides(reinterpret_cast<std::array<Py_ssize_t, PyBUF_MAX_NDIM>&>(*buffy->shape), _itemsize);
            for (size_t i=0; i<PyBUF_MAX_NDIM; i++)
                buffy->strides[i] = _strides[i];
            *buffy->padding = alignment - (shape_prod * _itemsize % alignment);
            buffy->ptr = (T*)aligned_alloc(alignment,shape_prod*_itemsize+*buffy->padding);
            //for (Py_ssize_t i=0; i<shape_prod; ++i)
            //    buffy->ptr[i] = 0;

        } else {
            abort();
        }
    }
    return (PyObject*)buffy;
}
void CustomBuffer_free(void *self) {
    printf("Free\n");
    CustomBuffer* buffy = (CustomBuffer*)self;
    free((void*)buffy->shape);
    free((void*)buffy->strides);
    free((void*)buffy->format);
    free((void*)buffy->ptr);
}
int CustomBuffer_init(PyObject* self, PyObject* args, PyObject* kwds) {
    printf("Init\n");
    CustomBuffer* buffy = (CustomBuffer*)self;
    return 0;
}
static char _doc_string[] = "My objects";
static PyType_Slot CustomBuffer_Slots[] = {
    {Py_tp_doc, _doc_string},
    {Py_tp_new, (void*)CustomBuffer_new},
    {Py_tp_init, (void*)CustomBuffer_init},
    {Py_tp_free, (void*)CustomBuffer_free},
    {Py_bf_getbuffer, (void*)get_buffer},
    {Py_bf_releasebuffer, (void*)release_buffer},
    {0, NULL}
};
static const char CustomBuffer_name[] = "CustomBuffer";
static const char CustomBuffer_doc_string[] = "Aligned Buffer with Buffer Protocol";
/*static PyType_Spec CustomBuffer_Spec = {
    .name = CustomBuffer_name,
    .basicsize = sizeof(CustomBuffer),
    .itemsize = (Py_ssize_t)sizeof(T),
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = CustomBuffer_Slots
};*/
static PyBufferProcs CustomBuffer_Procs = {&get_buffer, &release_buffer};
static PyTypeObject CustomBuffer_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) // typing.Callable
    CustomBuffer_name,
    sizeof(CustomBuffer),
    0,
    0, //(destructor)CustomBuffer_dealloc,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, //(ternaryfunc)CustomBuffer_call,
    0, 0, 0,
    &CustomBuffer_Procs,
    Py_TPFLAGS_DEFAULT,
    PyDoc_STR(CustomBuffer_doc_string),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    (initproc)CustomBuffer_init,
    PyType_GenericAlloc,
    (newfunc)CustomBuffer_new,
    (freefunc)CustomBuffer_free,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static struct PyModuleDef gpu_buffer_module = {
    PyModuleDef_HEAD_INIT,
    "gpu_buffer",
    "",
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};
PyMODINIT_FUNC PyInit_gpu_buffer(void){
    PyObject* mod = PyModule_Create(&gpu_buffer_module);
    if (PyModule_AddType(mod, &CustomBuffer_Type) < 0)
        abort();
    return mod;
}
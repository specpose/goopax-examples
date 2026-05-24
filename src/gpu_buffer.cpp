#include <goopax>
#include <goopax_extra/types.hpp>
#include <Python.h>
static Py_ssize_t queue_variables = 3;
static Py_ssize_t user_variables = 1;
#include "gpu_reshaper.hpp"
typedef double T;
typedef struct {
    PyObject_VAR_HEAD
    Py_ssize_t* shape;
    char* format;
    T* ptr;
} CustomBuffer;
int get_buffer(PyObject *exporter, Py_buffer *view, int flags){
    printf("GetBuffer\n");
    /*  TODO: Implement memoryview cast */
    //if ((flags & (PyBUF_C_CONTIGUOUS | PyBUF_FORMAT)) == (PyBUF_C_CONTIGUOUS | PyBUF_FORMAT)) {
        //init
        view->readonly = 0;
        view->suboffsets = NULL;
        view->shape = ((CustomBuffer*)exporter)->shape;
        view->format = ((CustomBuffer*)exporter)->format;
        T* ptr = ((CustomBuffer*)exporter)->ptr;
        view->buf = (void*)ptr;
        view->obj = exporter;

        //Get itemsize from format
        view->itemsize = itemsize(view->format);

        //Calculate strides from shape and itemsize
        /*  Hack: Allocation should be in CustomBuffer_new  */
        view->strides = (Py_ssize_t*)malloc(PyBUF_MAX_NDIM*sizeof(Py_ssize_t));
        auto strides = make_strides(reinterpret_cast<std::array<Py_ssize_t, PyBUF_MAX_NDIM>&>(*view->shape), view->itemsize);
        for (size_t i=0; i<PyBUF_MAX_NDIM; i++)
            view->strides[i] = strides[i];

        //Get ndim from shape
        view->ndim = ndim(view->shape);

        view->len = len(view->itemsize, view->shape);

    //} else {
    //    abort();
    //}
    return 0;
};
void release_buffer(PyObject *exporter, Py_buffer *view){
    printf("ReleaseBuffer\n");
    free((void*)view->strides);
};
PyObject *CustomBuffer_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds){
    printf("New\n");
    CustomBuffer* buffy = (CustomBuffer*)subtype->tp_alloc(subtype, 0);
    if (buffy != NULL) {
        buffy->shape = (Py_ssize_t*)malloc(PyBUF_MAX_NDIM*sizeof(Py_ssize_t));
        const char* format = NULL;
        Py_ssize_t format_length = 0;
        PyObject* shape = NULL;
        if (PyArg_ParseTuple(args, "s#O", &format, &format_length, &shape)) {
            buffy->format = (char*)malloc(sizeof(format_length));
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
            buffy->ptr = (T*)malloc(shape_prod*_itemsize);
            for (Py_ssize_t i=0; i<shape_prod; ++i)
                buffy->ptr[i] = 0;

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
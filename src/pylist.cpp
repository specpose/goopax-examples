#include <Python.h>
#include <stdio.h>

typedef double T;
static const T test_data[] = {0,0,1,0,1+sqrt(2)/2,sqrt(2)/2,1+sqrt(2)/2,1+sqrt(2)/2};

static void process(void* buffer, Py_ssize_t* strides, Py_ssize_t* shape){
    printf("shape=(%d,%d)\n",shape[0],shape[1]);
    T* _buffer = (T*)buffer;
    for (Py_ssize_t i=0; i<shape[0]*shape[1]; i+=2)
        printf("(%f,%f)\n",_buffer[i],_buffer[i+1]);
}

#define PY_SSIZE_T_CLEAN
#define Py_LIMITED_API

#if DEBUG
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

static const Py_ssize_t expected_vec_dims = 2;

static PyObject* pylist_process_list(PyObject* self, PyObject* args){
    PyObject* list = NULL;
    PyArg_ParseTuple(args, "O", &list);
    printf("%x: Retrieving list from tuple\n",list);
    Py_ssize_t shape_1 = PyList_Size(list);
    printf("list is size %d\n",shape_1);
    T* buffer;
    buffer = (T*)malloc(expected_vec_dims*sizeof(T)*shape_1);
    for(Py_ssize_t i=0; i<shape_1; i++){
        PyObject* item = PyList_GetItem(list,i);
        printf("%x: Retrieving vec2d from list\n",item);
        Py_ssize_t shape_2 = PyTuple_Size(item);
        if (shape_2 != expected_vec_dims)
            abort();
        for (Py_ssize_t j=0; j<shape_2; j++){
            PyObject* e = PyTuple_GetItem(item,j);
            printf("%x: Retrieving element %d from vec2d\n",e,j);
            buffer[shape_2*i+j]=PyFloat_AsDouble(e);
            printf("%f,\n",buffer[shape_2*i+j]);//T
        }
        Py_DECREF(item);//1
    }
    Py_DECREF(list);
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
    //return Py_RETURN_NONE;
}

PyObject* _vec2d(T x, T y){
    PyObject* result = PyTuple_New(expected_vec_dims);
    printf("%x: New Vec2d created\n",result);
    PyObject* x_ = PyFloat_FromDouble(x);//T
    PyObject* y_ = PyFloat_FromDouble(y);//T
    PyTuple_SetItem(result, 0, x_);
    PyTuple_SetItem(result, 1, y_);
    return result;
}

static PyObject* pylist_process_memoryview(PyObject* self, PyObject* args){
    PyObject* mvobject = NULL;
    PyArg_ParseTuple(args, "O", &mvobject);
    printf("%x: Retrieving memoryview from tuple\n",mvobject);
    if (PyMemoryView_Check(mvobject)!=1)
        abort();
    Py_buffer* view = PyMemoryView_GET_BUFFER(mvobject);
    process(view->buf,view->strides,view->shape);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* pylist_test_list(PyObject* self, PyObject* what){
    PyObject* list = PyList_New(0);
    printf("%x: New List of Vec2d created\n",list);
    for (size_t i=0;i<sizeof(test_data)/sizeof(T);i+=2){
        PyObject* item = _vec2d(test_data[i],test_data[i+1]);
        printf("%x: Passing Vec2d to list\n",item);
        PyList_Append(list, item);
    }
    PyObject* printList = PyObject_GetAttrString(self,"process_list");
    PyObject* args = PyTuple_New(1);
    printf("%x: New tuple with list created\n",args);
    printf("%x: Passing list to tuple\n",list);
    PyTuple_SetItem(args, 0, list);
    PyObject* result = PyObject_CallObject(printList, args);
    Py_DECREF(args);
    Py_DECREF(printList);
    return PyLong_FromLong(0);
}

typedef struct {
    PyObject_VAR_HEAD
    Py_ssize_t* shape;
    Py_ssize_t* strides;
    char* format;
    T* ptr;
} MyObject;
static const char _format[] = "d"; // T
int get_buffer(PyObject *exporter, Py_buffer *view, int flags){
    printf("GetBuffer\n");
    if ((flags & (PyBUF_C_CONTIGUOUS | PyBUF_FORMAT)) == (PyBUF_C_CONTIGUOUS | PyBUF_FORMAT)) {
        view->ndim = 2;
        Py_ssize_t* shape = ((MyObject*)exporter)->shape;
        shape[0]=4;
        shape[1]=2;
        Py_ssize_t* strides = ((MyObject*)exporter)->strides;
        strides[0]=16;
        strides[1]=8;
        view->shape = shape;
        view->strides = strides;
        view->suboffsets = NULL;
        view->format = ((MyObject*)exporter)->format;
        strcpy(view->format, _format);
        view->itemsize = sizeof(T);
        view->len = view->shape[0]*view->shape[1]*view->itemsize;
        T* ptr = ((MyObject*)exporter)->ptr;
        view->readonly = 1;
        view->buf = (void*)ptr;
        view->obj = exporter;
    } else {
        abort();
    }
    return 0;
};
void release_buffer(PyObject *exporter, Py_buffer *view){
    printf("ReleaseBuffer\n");
};
void myobj_dealloc(PyObject *self){
    printf("Dealloc\n");
    Py_TYPE(self)->tp_free(self);
};
PyObject *myobj_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds){
    printf("New\n");
    MyObject* buffy = (MyObject*)subtype->tp_alloc(subtype, 0);
    if (buffy != NULL) {
        buffy->shape = (Py_ssize_t*)malloc(PyBUF_MAX_NDIM*sizeof(Py_ssize_t));
        buffy->strides = (Py_ssize_t*)malloc(PyBUF_MAX_NDIM*sizeof(Py_ssize_t));
        buffy->format = (char*)malloc(sizeof(_format));
        buffy->ptr = (T*)malloc(sizeof(test_data)*sizeof(T));
    }
    return (PyObject*)buffy;
}
void myobj_free(void *self) {
    printf("Free\n");
    MyObject* buffy = (MyObject*)self;
    free((void*)buffy->shape);
    free((void*)buffy->strides);
    free((void*)buffy->format);
    free((void*)buffy->ptr);
}
int myobj_init(PyObject* self, PyObject* args, PyObject* kwds) {
    printf("Init\n");
    MyObject* buffy = (MyObject*)self;
    for (size_t i=0; i<sizeof(test_data)/sizeof(T); i++)
        buffy->ptr[i] = test_data[i];
    return 0;
}
static PyBufferProcs Buf_Procs = {
    &get_buffer,
    &release_buffer
};
static const char _name[] = "mymod.MyObject";
static const char _doc_string[] = "My objects";
static PyTypeObject MyObject_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    _name,       // .tp_name
    sizeof(MyObject),     // .tp_basicsize
    0,        // .tp_itemsize
    (destructor)myobj_dealloc,        // .tp_dealloc
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    &Buf_Procs,     // .tp_as_buffer
    Py_TPFLAGS_DEFAULT,      // .tp_flags
    PyDoc_STR(_doc_string),        // .tp_doc
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    (initproc)myobj_init,      // .tp_init
    PyType_GenericAlloc,      // .tp_alloc
    (newfunc)myobj_new,       // .tp_new
    (freefunc)myobj_free,       // .tp_free
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0       // ?
};

static PyObject* pylist_test_memoryview(PyObject* self, PyObject* what){
    if (PyType_Ready(&MyObject_Type) < 0)
        abort();
    PyObject* myobj = PyObject_CallObject((PyObject*)&MyObject_Type,NULL);  // .tp_new
    Py_buffer view;
    if (PyObject_GetBuffer((PyObject*)myobj,&view, PyBUF_C_CONTIGUOUS | PyBUF_FORMAT)==0){
        PyObject* mvobject = PyMemoryView_FromBuffer(&view);
        PyObject* printMemoryview = PyObject_GetAttrString(self,"process_memoryview");
        PyObject* args = PyTuple_New(1);
        printf("%x: New tuple with memoryview created\n",args);
        printf("%x: Passing memoryview to tuple\n",mvobject);
        PyTuple_SetItem(args, 0, mvobject);
        PyObject* result = PyObject_CallObject(printMemoryview, args);
        PyBuffer_Release(&view);
    }
    PyObject_Del((void*)myobj);
    return PyLong_FromLong(0);
}

static PyMethodDef pylist_methods[] = {
    {"process_list", pylist_process_list, METH_VARARGS, ""},
    {"process_memoryview", pylist_process_memoryview, METH_VARARGS, ""},
    {"test_list", pylist_test_list, METH_NOARGS, ""},
    {"test_memoryview", pylist_test_memoryview, METH_NOARGS, ""},
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

int main(){
    PyImport_AppendInittab("pylist", PyInit_pylist);
    Py_InitializeEx(0);

    PyObject* pylist = PyImport_ImportModule("pylist");
    PyObject* tl = PyObject_GetAttrString(pylist,"test_list");
    PyObject* tm = PyObject_GetAttrString(pylist,"test_memoryview");
    PyObject* unittest = PyImport_ImportModule("unittest");
    PyObject* tcase = PyObject_GetAttrString(unittest,"FunctionTestCase");
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
    PyObject* tests_tuple = PyTuple_New(2);
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
#if DEBUG
    if (Py_FinalizeEx() < 0)
        abort();
#else
    Py_Finalize();
#endif
    return tR>0&&e==0&&f==0&&uS==0 ? 0 : 1;
}

#if DEBUG
#undef Py_DECREF
#define Py_DECREF Py_DECREF_bak;
#endif
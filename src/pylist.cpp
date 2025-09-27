#define PY_SSIZE_T_CLEAN
#if !DEBUG
#define Py_LIMITED_API 1
#endif
#include <Python.h>
#include <stdio.h>

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

typedef double T;
static const size_t expected_vec_dims = 2;

static void process(T* buffer, size_t* strides, size_t* shape, size_t length){}
static void bufferUnderrun(size_t expected_size, size_t size_obtained){ }

static PyObject* pylist_process_list(PyObject* self, PyObject* args){
    PyObject* list = NULL;
    PyArg_ParseTuple(args, "O", &list);
    printf("%x: Retrieving list from tuple\n",list);
    size_t shape_1 = PyList_Size(list);
    printf("list is size %d\n",shape_1);
    T* buffer;
    buffer = (T*)malloc(expected_vec_dims*sizeof(T)*shape_1);
    for(size_t i=0; i<shape_1; i++){
        PyObject* item = PyList_GetItem(list,i);
        printf("%x: Retrieving vec2d from list\n",item);
        size_t shape_2 = PyTuple_Size(item);
        if (shape_2 != expected_vec_dims)
            abort();
        for (size_t j=0; j<shape_2; j++){
            PyObject* e = PyTuple_GetItem(item,j);
            printf("%x: Retrieving element %d from vec2d\n",e,j);
            buffer[shape_2*i+j]=PyFloat_AsDouble(e);
            printf("%f,\n",buffer[shape_2*i+j]);//T
        }
        Py_DECREF(item);//1
    }
    Py_DECREF(list);
    //process
    free(buffer);
    Py_INCREF(Py_None);
    return Py_None;
    //return Py_RETURN_NONE;
}

typedef struct {
    T x,y;
} vec2d;
PyObject* _vec2d(vec2d v){
    PyObject* result = PyTuple_New(expected_vec_dims);
    printf("%x: New Vec2d created\n",result);
    PyObject* x = PyFloat_FromDouble(v.x);//T
    PyObject* y = PyFloat_FromDouble(v.y);//T
    PyTuple_SetItem(result, 0, x);
    PyTuple_SetItem(result, 1, y);
    return result;
}
//vec2d operator+(const vec2d& a, const vec2d& b){ vec2d c; c.x=a.x+b.x; c.y=a.y+b.y; return c;}

static PyObject* pylist_process_memoryview(PyObject* self, PyObject* args){
    PyObject* mvobject = NULL;
    PyArg_ParseTuple(args, "O", &mvobject);
    printf("%x: Retrieving memoryview from tuple\n",mvobject);
    Py_buffer* view = PyMemoryView_GET_BUFFER(mvobject);
    //Py_buffer* view = (Py_buffer*)mvobject;
    for (int i=0; i<8; i+=2)
        printf("(%f,%f)\n",((T*)view->buf)[i],((T*)view->buf)[i+1]);
    //process
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* pylist_test_list(PyObject* self, PyObject* what){
    PyObject* list = PyList_New(0);
    vec2d test[] = {{0,0},{1,0},{sqrt(2)/2,sqrt(2)/2},{0,1}};
    const size_t test_size = sizeof(test)/sizeof(vec2d);
    for (size_t i=1;i<test_size;i++){
        test[i].x=test[i-1].x+test[i].x;
        test[i].y=test[i-1].y+test[i].y;
    }
    printf("%x: New List of Vec2d created\n",list);
    for (size_t i=0;i<test_size;i++){
        PyObject* item = _vec2d(test[i]);
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

#if DEBUG
typedef struct {
    PyObject_HEAD
} MyObject;
int get_buffer(PyObject *exporter, Py_buffer *view, int flags){
    printf("GetBuffer\n");
    vec2d test[] = {{0,0},{1,0},{sqrt(2)/2,sqrt(2)/2},{0,1}};
    Py_ssize_t* shape = (Py_ssize_t*)malloc(2*sizeof(T));
    shape[0]=3;
    shape[1]=2;
    Py_ssize_t* strides = (Py_ssize_t*)malloc(2*sizeof(T));
    strides[0]=16;
    strides[1]=8;
    view->shape = shape;
    view->strides = strides;
    view->suboffsets = NULL;
    vec2d* ptr = (vec2d*)malloc(8*sizeof(T));
    if (!ptr)
        abort();
    for (int i=0; i<4; i++)
        ptr[i] = test[i];
    view->readonly = 1;
    view->format = NULL;
    view->buf = (void*)ptr;
    PyObject* _exporter = exporter;
    view->obj = _exporter;
    return 0;
};
void release_buffer(PyObject *exporter, Py_buffer *view){
    printf("ReleaseBuffer\n");
    free((void*)view->shape);
    free((void*)view->strides);
    free((void*)view->buf);
};
/*void myobj_dealloc(PyObject *self){ printf("Dealloc\n"); };
PyObject *myobj_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds){
    printf("Alloc\n");
    return NULL;
};*/
static PyBufferProcs Buf_Procs = {
    &get_buffer,
    &release_buffer
};
static PyTypeObject MyObject_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "mymod.MyObject",
    .tp_basicsize = sizeof(MyObject),
//    .tp_itemsize = 0,
//    .tp_dealloc = (destructor)myobj_dealloc,
    .tp_as_buffer = &Buf_Procs,
    .tp_doc = PyDoc_STR("My objects"),
//    .tp_init = 0,
//    .tp_alloc = 0,
//    .tp_new = myobj_new,
};
#endif

static PyObject* pylist_test_memoryview(PyObject* self, PyObject* what){
#if DEBUG
    PyType_Ready(&MyObject_Type);
    MyObject* myobj = PyObject_New(MyObject,&MyObject_Type);
//    PyObject* myobj_ = PyObject_Init((PyObject*)myobj, &MyObject_Type);
    Py_buffer view;
    if (PyObject_GetBuffer((PyObject*)myobj,&view,PyBUF_STRIDES)==0){
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
#endif
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
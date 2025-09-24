#define PY_SSIZE_T_CLEAN
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
#endif

#if DEBUG
#undef Py_DECREF
#define Py_DECREF(X) __delete(X);
#endif

typedef double T;

static PyObject* pylist_print_list(PyObject* self, PyObject* args){
    PyObject* list = NULL;
    PyArg_ParseTuple(args, "O", &list);
    printf("%x: Retrieving list from tuple\n",list);
    int size = PyList_Size(list);
    printf("list is size %d\n",size);
    for(int i=0; i<size; i++){
        PyObject* item = PyList_GetItem(list,i);
        printf("%x: Retrieving vec2d from list\n",item);
        PyObject* x = PyTuple_GetItem(item,0);
        printf("%x: Retrieving x from vec2d\n",x);
        PyObject* y = PyTuple_GetItem(item,1);
        Py_DECREF(item);//1
        printf("%x: Retrieving y from vec2d\n",y);
        printf("(%f,%f),\n",PyFloat_AsDouble(x),PyFloat_AsDouble(y));//T
        Py_DECREF(x);
        Py_DECREF(y);
    }
    Py_DECREF(list);
    Py_INCREF(Py_None);
    return Py_None;
    //return Py_RETURN_NONE;
}

typedef struct {
    T x,y;
} vec2d;
PyObject* _vec2d(vec2d v){
    PyObject* result = PyTuple_New(2);
    printf("%x: New Vec2d created\n",result);
    PyObject* x = PyFloat_FromDouble(v.x);//T
    PyObject* y = PyFloat_FromDouble(v.y);//T
    PyTuple_SetItem(result, 0, x);
    PyTuple_SetItem(result, 1, y);
    return result;
}
//vec2d operator+(const vec2d& a, const vec2d& b){ vec2d c; c.x=a.x+b.x; c.y=a.y+b.y; return c;}

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
    PyObject* printList = PyObject_GetAttrString(self,"print_list");
    PyObject* args = PyTuple_New(1);
    printf("%x: New tuple with list created\n",args);
    printf("%x: Passing list to tuple\n",list);
    PyTuple_SetItem(args, 0, list);
    PyObject* result = PyObject_CallObject(printList, args);
    Py_DECREF(args);
    Py_DECREF(printList);
    return PyLong_FromLong(0);
}

static PyMethodDef pylist_methods[] = {
    {"print_list", pylist_print_list, METH_VARARGS, ""},
    {"test_list", pylist_test_list, METH_NOARGS, ""},
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
    return PyModuleDef_Init(&pylist_module);
}

int main(){
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    PyImport_AppendInittab("pylist", PyInit_pylist);
    Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);

    PyObject* pylist = PyImport_ImportModule("pylist");
    PyObject* pytest = PyObject_GetAttrString(pylist,"test_list");
    //Py_DECREF(pylist);//3
    PyObject* unittest = PyImport_ImportModule("unittest");
    PyObject* cls = PyObject_GetAttrString(unittest,"FunctionTestCase");
    Py_DECREF(unittest);//1
    PyObject* args = PyTuple_New(1);
    PyTuple_SetItem(args, 0, pytest);
    Py_DECREF(pytest);//1
    //Py_DECREF(pylist);//2
    PyObject* constructor = PyObject_CallObject(cls,args);
    Py_DECREF(args);
    Py_DECREF(pylist);//1
    //Py_DECREF(cls);//5
    //PyObject* result = PyObject_CallObject(pytest,NULL);
    PyObject* result = PyObject_CallObject(constructor,NULL);//segfaults
    Py_DECREF(constructor);
    //Py_DECREF(cls);//3
    PyObject* times = PyObject_GetAttrString(result,"collectedDurations");
    //Py_DECREF(cls);//2
    for (int i=0;i<PyList_Size(times);i++) {
        PyObject* tindex = PyList_GetItem(times,i);
        Py_DECREF(times);//1
        printf("%s took %f.\n",PyTuple_GetItem(tindex,0),PyFloat_AsDouble(PyTuple_GetItem(tindex,1)));
        Py_DECREF(tindex);
    }
    Py_DECREF(times);//0
    int uS = PyList_Size(PyObject_GetAttrString(result,"unexpectedSuccesses"));
    int f = PyList_Size(PyObject_GetAttrString(result,"failures"));
    int e = PyList_Size(PyObject_GetAttrString(result,"errors"));
    long tR = PyLong_AsLong(PyObject_GetAttrString(result,"testsRun"));
    Py_DECREF(result);
    Py_DECREF(cls);//1
    if (Py_FinalizeEx() < 0)
        abort();
    return tR>0&&e==0&&f==0&&uS==0 ? 0 : 1;
}

#if DEBUG
#undef Py_DECREF
#define Py_DECREF Py_DECREF_bak;
#endif
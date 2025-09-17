#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdio.h>

#if DEBUG
#undef Py_DECREF
#define Py_DECREF(X) free(X)
#endif

typedef double T;

static PyObject* goopax_import_list(PyObject* self, PyObject* args){
    PyObject* list;
    PyArg_ParseTuple(args, "O", &list);
    printf("%x: Retrieving list from typle\n",list);
    int size = PyList_Size(list);
    printf("list is size %d\n",size);
    PyObject* item;
    PyObject* x;
    PyObject* y;
    for(int i=0; i<size; i++){
        item = PyList_GetItem(list,i);
        printf("%x: Retrieving vec2d from list\n",item);
        x = PyTuple_GetItem(item,0);
        printf("%x: Retrieving x from vec2d\n",x);
        y = PyTuple_GetItem(item,1);
        printf("%x: Retrieving y from vec2d\n",y);
        printf("(%f,%f),\n",PyFloat_AsDouble(x),PyFloat_AsDouble(y));//T
    }
    Py_DECREF(item);
    Py_DECREF(x);
    Py_DECREF(y);
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

static PyObject* goopax_test_list(PyObject* self, PyObject* what){
    PyObject* list = PyList_New(0);
    vec2d test[] = {{0,0},{1,0},{sqrt(2)/2,sqrt(2)/2},{0,1}};
    const size_t test_size = sizeof(test)/sizeof(vec2d);
    for (size_t i=1;i<test_size;i++){
        test[i].x=test[i-1].x+test[i].x;
        test[i].y=test[i-1].y+test[i].y;
    }
    printf("%x: New List of Vec2d created\n",list);
    PyObject* item;
    for (size_t i=0;i<test_size;i++){
        item = _vec2d(test[i]);
        printf("%x: Passing Vec2d to list\n",item);
        PyList_Append(list, item);
        //Py_DECREF(item);
    }
    PyObject* printList = PyObject_GetAttrString(self,"import_list");
    PyObject* args = PyTuple_New(1);
    printf("%x: New tuple with list created\n",args);
    printf("%x: Passing list to typle\n",list);
    PyTuple_SetItem(args, 0, list);
    PyObject* result = PyObject_CallObject(printList, args);
    Py_DECREF(args);
    Py_DECREF(printList);
    Py_DECREF(list);
    Py_DECREF(item);
    Py_DECREF(result);
    return PyLong_FromLong(0);
}

static PyMethodDef goopax_methods[] = {
    {"import_list", goopax_import_list, METH_VARARGS, ""},
    {"test_list", goopax_test_list, METH_NOARGS, ""},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef goopax_module = {
    PyModuleDef_HEAD_INIT,
    "goopax",
    "",
    0,
    goopax_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC PyInit_goopax(void){
    return PyModuleDef_Init(&goopax_module);
}

int main(){
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    PyImport_AppendInittab("goopax", PyInit_goopax);
    Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);

    PyObject* pmodule = PyImport_ImportModule("goopax");
    PyObject* pytest = PyObject_GetAttrString(pmodule,"test_list");
    PyObject* noargs = PyTuple_New(0);
    PyObject* manual_invocation = PyObject_CallObject(pytest, noargs);

    Py_DECREF(noargs);
    Py_DECREF(pytest);
    Py_DECREF(manual_invocation);
    Py_DECREF(pmodule);
    return 0;
}
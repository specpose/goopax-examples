#define PY_SSIZE_T_CLEAN
#include <Python.h>

static PyObject* goopax_import_list(PyObject* self, PyObject* args){
    return PyLong_FromLong(0);
}

static PyMethodDef goopax_methods[] = {
    {"import_list", goopax_import_list, METH_VARARGS, ""},
    {NULL, NULL, 0, NULL}
};

//static PyObject *GoopaxError = NULL;
static int goopax_module_exec(PyObject* m){
//    PyModule_AddObjectRef(m, "GoopaxError", GoopaxError);
    return 0;
}

static PyModuleDef_Slot goopax_module_slots[] = {
//    {Py_mod_exec, goopax_module_exec},
    {0, NULL}
};

static struct PyModuleDef goopax_module = {
    PyModuleDef_HEAD_INIT,
    "goopax",
    "",
    0,
    goopax_methods,
//    goopax_module_slots,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC PyInit_goopax(void){
    return PyModuleDef_Init(&goopax_module);
}

int main(){
    //PyStatus status;
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    //PyImport_AppendInittab("goopax", PyInit_goopax);
    Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    PyObject *pmodule = PyImport_ImportModule("goopax");
    //if (!pmodule)
    //    PyErr_Print();
    delete pmodule;
    return 0;
}
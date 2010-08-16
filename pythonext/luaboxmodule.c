#include <Python.h>

/* Exception type, used for all errors that can occur in this module. */
static PyObject *LuaBoxError;

/* the sandbox type */
typedef struct {
	PyObject_HEAD
} luabox_SandboxObject;

static PyTypeObject luabox_SandboxType = {
	PyObject_HEAD_INIT(NULL)
	0,                             /*ob_size*/
	"luabox.Sandbox",              /*tp_name*/
	sizeof(luabox_SandboxObject),  /*tp_basicsize*/
	0,                             /*tp_itemsize*/
	0,                             /*tp_dealloc*/
	0,                             /*tp_print*/
	0,                             /*tp_getattr*/
	0,                             /*tp_setattr*/
	0,                             /*tp_compare*/
	0,                             /*tp_repr*/
	0,                             /*tp_as_number*/
	0,                             /*tp_as_sequence*/
	0,                             /*tp_as_mapping*/
	0,                             /*tp_hash */
	0,                             /*tp_call*/
	0,                             /*tp_str*/
	0,                             /*tp_getattro*/
	0,                             /*tp_setattro*/
	0,                             /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,            /*tp_flags*/
	"Lua sandbox. Document me",    /* tp_doc */
};

/* example function, needs to be removed at some point */
static PyObject *luabox_hello(PyObject *self, PyObject *args) {
	const char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	printf("Hello, %s!\n", name);

	PyErr_SetString(LuaBoxError, "LOL, error!");
	return NULL;

	return Py_BuildValue("i", 99);
}

/* metadata about methods */
PyMethodDef LuaBoxMethods[] = {
	{"hello", luabox_hello, METH_VARARGS, "Say hello!"},
	{NULL, NULL, 0, NULL},
};

/* module initialization */
PyMODINIT_FUNC initluabox(void) {
	PyObject *m;

	m = Py_InitModule("luabox", LuaBoxMethods);
	if (m == NULL) return;

	/* set up exception type */
	LuaBoxError = PyErr_NewException("luabox.error", NULL, NULL);
	Py_INCREF(LuaBoxError);
	PyModule_AddObject(m, "error", LuaBoxError);

	/* add sandbox type */
	luabox_SandboxType.tp_new = PyType_GenericNew;
	if(PyType_Ready(&luabox_SandboxType) < 0) return;

	Py_INCREF(&luabox_SandboxType);
	PyModule_AddObject(m, "Sandbox", (PyObject*) &luabox_SandboxType);
}

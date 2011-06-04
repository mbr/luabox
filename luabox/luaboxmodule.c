#include "luaboxmodule.h"

PyObject *Exc_LuaBoxException;
PyObject *Exc_OutOfMemory;
PyObject *Exc_SyntaxError;
PyObject *Exc_RuntimeError;
PyObject *Exc_ErrorError;

/* module initialization */
PyMODINIT_FUNC initluabox(void) {
	PyObject *m;

	m = Py_InitModule("luabox", NULL);
	if (m == NULL) return;

	/* set up exception types */
	Exc_LuaBoxException = PyErr_NewException("luabox.LuaBoxException", NULL, NULL);
	Py_XINCREF(Exc_LuaBoxException);
	PyModule_AddObject(m, "LuaBoxException", Exc_LuaBoxException);

	Exc_OutOfMemory = PyErr_NewException("luabox.OutOfMemory", Exc_LuaBoxException, NULL);
	Py_XINCREF(Exc_OutOfMemory);
	PyModule_AddObject(m, "OutOfMemory", Exc_OutOfMemory);

	Exc_SyntaxError = PyErr_NewException("luabox.SyntaxError", Exc_LuaBoxException, NULL);
	Py_XINCREF(Exc_SyntaxError);
	PyModule_AddObject(m, "SyntaxError", Exc_SyntaxError);

	Exc_RuntimeError = PyErr_NewException("luabox.RuntimeError", Exc_LuaBoxException, NULL);
	Py_XINCREF(Exc_RuntimeError);
	PyModule_AddObject(m, "RuntimeError", Exc_RuntimeError);

	Exc_ErrorError = PyErr_NewException("luabox.ErrorError", Exc_LuaBoxException, NULL);
	Py_XINCREF(Exc_ErrorError);
	PyModule_AddObject(m, "ErrorError", Exc_ErrorError);

	SandboxType_INIT(&SandboxType);
	LuaTableRefType_INIT(&LuaTableRefType);

	Py_XINCREF(&SandboxType);
	Py_XINCREF(&LuaTableRefType);
	PyModule_AddObject(m, "Sandbox", (PyObject*) &SandboxType);
	PyModule_AddObject(m, "LuaTableRef", (PyObject*) &LuaTableRefType);

	/* add some constants */
	PyModule_AddIntConstant(m, "LUA_MULTRET", LUA_MULTRET);
}

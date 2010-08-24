#include "luaboxmodule.h"

PyTypeObject LuaTableRefType = {
	PyObject_HEAD_INIT(NULL)
	0,                                  /*ob_size*/
	"luabox.LuaTableRef",               /*tp_name*/
	sizeof(LuaTableRef)                    /*tp_basicsize*/
};

static void LuaTableRef_dealloc(LuaTableRef *self) {
	if (-1 != self->ref) {
		/* free lua ref */
		luaL_unref(self->sandbox->L, LUA_REGISTRYINDEX, self->ref);
	}
	Py_DECREF(self->sandbox);
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject* LuaTableRef_str(PyObject *self) {
	LuaTableRef* ltr = (LuaTableRef*) self;
	return PyString_FromFormat("<LuaTableRef ref:%d>", ltr->ref);
}

void LuaTableRefType_INIT(PyTypeObject *t) {
	/* add sandbox type */
	t->tp_new = 0; // disallow constructing type instances
	t->tp_dealloc = (destructor)LuaTableRef_dealloc;
	t->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
	t->tp_doc = "Reference to lua table in lua sandbox.";
	t->tp_str = LuaTableRef_str;

	if(PyType_Ready(t) < 0) return;
}

PyObject *LuaTableRef_from_stack(Sandbox *sandbox) {
	LuaTableRef *ltr = PyObject_New(LuaTableRef, &LuaTableRefType);

	/* hold a reference to the sandbox */
	Py_INCREF(sandbox);
	ltr->sandbox = sandbox;

	/* create a reference to table, pops it from the stack */
	ltr->ref = luaL_ref(ltr->sandbox->L, LUA_REGISTRYINDEX);

	if (-1 == ltr->ref) {
		Py_DECREF(ltr);
		PyErr_SetString(Exc_RuntimeError, "Could not create LuaTableRef. Empty stack?");
		return NULL;
	}

	return (PyObject*)ltr;
}

#include "luaboxmodule.h"

PyMappingMethods LuaTableRef_mapping = {
	NULL,        /* mp_length */
	NULL,        /* mp_subscript */
	NULL         /* mp_ass_subscript */

	/* methods assigned in LuaTableRefType_INIT */
};

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

static Py_ssize_t LuaTableRef_length(PyObject *self) {
	LuaTableRef *ltr = (LuaTableRef*) self;

	/* push table onto stack */
	lua_rawgeti(ltr->sandbox->L, LUA_REGISTRYINDEX, ltr->ref);

	/* get length */
	size_t len = lua_objlen(ltr->sandbox->L, -1);

	/* pop table from stack */
	lua_pop(ltr->sandbox->L, 1);

	return (Py_ssize_t) len;
}

static PyObject* LuaTableRef_str(PyObject *self) {
	LuaTableRef* ltr = (LuaTableRef*) self;
	return PyString_FromFormat("<LuaTableRef ref:%d>", ltr->ref);
}

static PyObject* LuaTableRef_subscript(PyObject *self, PyObject *key) {
	LuaTableRef* ltr = (LuaTableRef*) self;

	/* get table, push onto stack */
	lua_rawgeti(ltr->sandbox->L, LUA_REGISTRYINDEX, ltr->ref);

	/* convert key to lua value, put on top of stack */
	if (! python_to_lua(ltr->sandbox->L, key)) return NULL;

	/* look up key on table (this will pop the key) */
	lua_gettable(ltr->sandbox->L, -2); // -2  table,  -1  key

	/* convert retrieved value on top of stack to python value */
	PyObject *rval = Sandbox_pop(ltr->sandbox, NULL);
	if (NULL == rval) {
		/* no pop occured, discard value and key */
		lua_pop(ltr->sandbox->L, 2);
		return NULL;
	}

	/* pop table */
	lua_pop(ltr->sandbox->L, 1);

	return rval;
}

void LuaTableRefType_INIT(PyTypeObject *t) {
	/* add sandbox type */
	t->tp_new = 0; // disallow constructing type instances
	t->tp_dealloc = (destructor)LuaTableRef_dealloc;
	t->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
	t->tp_doc = "Reference to lua table in lua sandbox.";
	t->tp_str = LuaTableRef_str;

	t->tp_as_mapping = &LuaTableRef_mapping;
	LuaTableRef_mapping.mp_length = LuaTableRef_length;
	LuaTableRef_mapping.mp_subscript = LuaTableRef_subscript;

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

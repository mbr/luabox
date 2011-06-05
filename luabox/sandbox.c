/**
 * Lua basic Sandbox.
 *
 * Contains all necessary code that implements the Sandbox class from the
 * luabox module.
 *
 * The Sandbox module aims to tightly wrap a lua_State as possible. The
 * C extension provides no syntactic sugar, which is added as Python code
 * on a class that extends it.
 */

#include "luaboxmodule.h"

PyTypeObject SandboxType = {
	PyObject_HEAD_INIT(NULL)
	0,                                  /*ob_size*/
	"luabox.Sandbox",                   /*tp_name*/
	sizeof(Sandbox)                    /*tp_basicsize*/

	/* other members initialized in SandboxType_INIT */
};

/* forward declarations */
static int Sandbox_setmemory_limit(Sandbox *self, PyObject *value, void *closure);

/* lua allocation function that enforces memory limit */
static void *lua_sandbox_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
	Sandbox *box = (Sandbox*) ud;
	void *nptr;

	size_t newmem_size = nsize-osize;
//	printf("current limit: %lu\n", box->lua_max_mem);
	if (nsize == 0) {
		/* a free is always allowed */
		free(ptr);
		nptr = NULL;
	} else {
		/* check if we are allowed to consume that much memory */
		if (0 != box->lua_max_mem && box->lua_max_mem < box->lua_current_mem+newmem_size) {
			/* too much memory used! */
//			printf("Memory denied! Would-be size: %ld\n", box->lua_current_mem+newmem_size);
//
			return NULL;
		} else {
			/* all good, allocate */
			nptr = realloc(ptr, nsize);
		}
	}

	/* update memory count */
	box->lua_current_mem += newmem_size;
//	printf("memory usage now: %lu\n", box->lua_current_mem);
	return nptr;
}

/* update the exception message by popping the stack */
static const char *luabox_exception_message(Sandbox *self) {
	if (self->lua_error_msg) free(self->lua_error_msg);
	
	/* save a copy of the lua error message */
	self->lua_error_msg = strdup(lua_tostring(self->L, -1));
	if (! self->lua_error_msg) self->lua_error_msg = "An exception message should be shown here, but there was not enough memory to allocate it.";

	lua_pop(self->L, -1);
	return self->lua_error_msg;
}

static int lua_sandbox_panic(lua_State *L) {
	printf("Panic handler called.\n");
	printf("Error: %s\n", lua_tostring(L, -1));

	return 0;
}

/**** Sandbox-METHODS ****/
static void Sandbox_dealloc(Sandbox *self) {
	if (self->L) lua_close(self->L);
	if (self->lua_error_msg) free(self->lua_error_msg);
	self->ob_type->tp_free((PyObject*)self);
}

/* memory_limit (lua_max_mem) setter */
static PyObject *Sandbox_getmemory_limit(Sandbox *self, void *closure) {
	return Py_BuildValue("K", self->lua_max_mem);
}

static PyObject* Sandbox_gettop(Sandbox *self, PyObject *args) {
	return Py_BuildValue("i", lua_gettop(self->L));
}

static int Sandbox_init(Sandbox *self, PyObject *args, PyObject *kwds) {
	return 0;
}

static PyObject* Sandbox_loadstring(Sandbox *self, PyObject *args, PyObject *kwds) {
	const char *s;
	static char *kwlist[] = {"s", NULL};

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &s)) {
		PyErr_SetString(PyExc_Exception, "Error parsing arguments.");
		return NULL;
	}

	switch(luaL_loadstring(self->L, s)) {
		case 0: break; /* no error */

		case LUA_ERRSYNTAX:
			/* pop error from stack */
			PyErr_SetString(Exc_SyntaxError, luabox_exception_message(self));
			return NULL;

		case LUA_ERRMEM:
			PyErr_SetString(Exc_OutOfMemory, luabox_exception_message(self));
			return NULL;

		default:
			PyErr_SetString(Exc_LuaBoxException, luabox_exception_message(self));
			return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* Sandbox_loadfile(Sandbox *self, PyObject *args, PyObject *kwds) {
	const char *filename;
	static char *kwlist[] = {"filename", NULL};

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &filename)) {
		PyErr_SetString(PyExc_Exception, "Error parsing arguments.");
		return NULL;
	}

	switch(luaL_loadfile(self->L, filename)) {
		case 0: break; /* no error */

		case LUA_ERRSYNTAX:
			/* pop error from stack */
			PyErr_SetString(Exc_SyntaxError, luabox_exception_message(self));
			return NULL;

		case LUA_ERRMEM:
			PyErr_SetString(Exc_OutOfMemory, luabox_exception_message(self));
			return NULL;

		case LUA_ERRFILE:
			PyErr_SetString(PyExc_IOError, luabox_exception_message(self));
			return NULL;

		default:
			PyErr_SetString(Exc_LuaBoxException, luabox_exception_message(self));
			return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* Sandbox_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
	Sandbox *self = (Sandbox*) type->tp_alloc(type, 0);
	self->lua_error_msg = 0;

	if(self) {
		PyObject *memory_limit = 0;
		static char *kwlist[] = {"memory_limit", NULL};
		if(! PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &memory_limit)) return NULL;

		/* memory_limit is zero if not supplied */
		if (! memory_limit) self->lua_max_mem = 0;
		else if (-1 == Sandbox_setmemory_limit(self, memory_limit, NULL)) return NULL;

		/* initialize lua_state */
		self->L = lua_newstate(lua_sandbox_alloc, self);

		if (! self->L) {
			/* not enough memory for the interpreter itself */
			PyErr_SetString(Exc_OutOfMemory, "Could not instantiate lua state.");
			Py_XDECREF(self);
			return NULL;
		}

		/* set panic function */
		lua_atpanic(self->L, lua_sandbox_panic);
	}

	return (PyObject*)self;
}

static PyObject* Sandbox_pcall(Sandbox *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"nargs", "nresults", "errfunc", NULL};
	int nargs = 0, nresults = 0, errfunc = 0;
	if (! PyArg_ParseTupleAndKeywords(args, kwds, "|iii", kwlist, &nargs, &nresults, &errfunc)) {
		PyErr_SetString(PyExc_Exception, "Error parsing arguments.");
		return NULL;
	}

	switch(lua_pcall(self->L, nargs, nresults, errfunc)) {
		case 0: break;

		case LUA_ERRRUN:
			PyErr_SetString(Exc_RuntimeError, luabox_exception_message(self));
			return NULL;

		case LUA_ERRMEM:
			PyErr_SetString(Exc_OutOfMemory, luabox_exception_message(self));
			return NULL;

		case LUA_ERRERR:
			PyErr_SetString(Exc_ErrorError, luabox_exception_message(self));
			return NULL;

		default:
			PyErr_SetString(Exc_LuaBoxException, luabox_exception_message(self));
			return NULL;
	}

	Py_RETURN_NONE;
}

PyObject* Sandbox_pop(Sandbox *self, PyObject *args) {
	const int index = -1;

	PyObject *rval;
	int t = lua_type(self->L, index);
	switch(t) {
		case LUA_TTABLE:
			rval = LuaTableRef_from_stack(self);
			return rval;

		default:
			rval = lua_to_python(self->L);
			if (! rval) return NULL; 
			lua_pop(self->L, 1);
			return rval;
	}

}

/* memory_limit (lua_max_mem) getter */
static int Sandbox_setmemory_limit(Sandbox *self, PyObject *value, void *closure) {
	if (! value) {
		PyErr_SetString(PyExc_TypeError, "Cannot delete memory limit.");
		return -1;
	}

	if (! PyInt_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "Memory limit must be an integer.");
		return -1;
	}

	self->lua_max_mem = PyInt_AsSsize_t(value);

	return 0;
}

static PyGetSetDef Sandbox_getseters[] = {
	{"memory_limit", (getter)Sandbox_getmemory_limit, (setter)Sandbox_setmemory_limit, "maximum allowed script memory usage (in bytes)", NULL},
	{NULL}
};

/* class member definition */
static PyMemberDef Sandbox_members[] = {
	{NULL}
};

static PyMethodDef Sandbox_methods[] = {
	{"gettop", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_gettop, METH_NOARGS, "get number of elements in stack"},
	{"loadfile", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_loadfile, METH_KEYWORDS, "load a file"},
	{"loadstring", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_loadstring, METH_KEYWORDS, "load a string"},
	{"pcall", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_pcall, METH_KEYWORDS, "protected function call"},
	{"pop", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_pop, METH_NOARGS, "pop and return"},
	{NULL}
};

void SandboxType_INIT(PyTypeObject *t) {
	/* add sandbox type */
	t->tp_dealloc = (destructor)Sandbox_dealloc;
	t->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
	t->tp_doc = "Lua sandbox. Documentation sadly lacking.";
	t->tp_methods = Sandbox_methods;
	t->tp_members = Sandbox_members;
	t->tp_getset = Sandbox_getseters;
	t->tp_init = (initproc)Sandbox_init;
	t->tp_new = Sandbox_new;

	if(PyType_Ready(t) < 0) return;
}

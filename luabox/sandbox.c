/**
 * Lua basic Sandbox.
 *
 * Contains all necessary code that implements the Sandbox class from the
 * luabox module.
 *
 * The Sandbox module aims to tightly wrap a lua_State as possible. The
 * C extension provides no syntactic sugar, which is added as Python code
 * on a class that extends it.
 *
 * In addition to that, the Sandbox uses a custom malloc() implementation
 * that allows defining bounds on how much memory the lua interpreter
 * may allocate for a specific lua_State instance. This is configurable
 * as the memory_limit parameter.
 */
#include "luaboxmodule.h"

PyTypeObject SandboxType = {
	PyObject_HEAD_INIT(NULL)
	0,                                  /*ob_size*/
	"luabox.Sandbox",                   /*tp_name*/
	sizeof(Sandbox)                     /*tp_basicsize*/

	/* The other members are initialized in SandboxType_INIT */
};

/* Forward declarations, as we don't use a sandbox.h header file. */
static int Sandbox_setmemory_limit(Sandbox *self, PyObject *value, void *closure);

/**
 * Memory allocator for lua, that enforces a hard memory limit.
 *
 * \param ud "User data", a pointer to a Sandbox object. The sandbox's
 *           `lua_max_mem` property will be used as the maximum allowed
 *           allocated memory size in bytes.
 *
 * For other parameters, see the documentation of lua_Alloc.
 */
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

/**
 * Pop the stack to get an error message from lua.
 *
 * A string containing an error message from lua, or an error message
 * explaining why the actual error message could not be fetched.
 *
 * \note The errormessage is stored in a static buffer on the 
 *       Sandbox. Subsequent calls to this function will invalidate
 *       the pointer from previous calls.
 */
static const char *luabox_exception_message(Sandbox *self) {
	if (self->lua_error_msg) free(self->lua_error_msg);
	
	/* save a copy of the lua error message */
	self->lua_error_msg = strdup(lua_tostring(self->L, -1));
	if (! self->lua_error_msg) self->lua_error_msg = "An exception message should be shown here, but there was not enough memory to allocate it.";

	lua_pop(self->L, -1);
	return self->lua_error_msg;
}

/**
 * lua panic function.
 *
 * Outputs the panic message to stdout.
 *
 * \note FIXME: this really should be handed back to python as an exception.
 */
static int lua_sandbox_panic(lua_State *L) {
	printf("Panic handler called.\n");
	printf("Error: %s\n", lua_tostring(L, -1));

	return 0;
}

/**
 * Deallocation method for python object.
 */
static void Sandbox_dealloc(Sandbox *self) {
	if (self->L) lua_close(self->L);
	if (self->lua_error_msg) free(self->lua_error_msg);
	self->ob_type->tp_free((PyObject*)self);
}

/**
 * Getter for memory_limit (see lua_max_mem).
 */
static PyObject *Sandbox_getmemory_limit(Sandbox *self, void *closure) {
	return Py_BuildValue("K", self->lua_max_mem);
}

/**
 * Returns the index of the top element of the lua stack.
 * \see lua_gettop.
 *
 * Python signature: gettop()
 */
static PyObject* Sandbox_gettop(Sandbox *self, PyObject *args) {
	return Py_BuildValue("i", lua_gettop(self->L));
}

/**
 * Init function for python object.
 */
static int Sandbox_init(Sandbox *self, PyObject *args, PyObject *kwds) {
	return 0;
}

/**
 * Load lua code from string.
 *
 * \see luaL_loadstring.
 *
 * Python signature: loadstring(s)
 */
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

/**
 * Load lua code from file.
 *
 * \see luaL_loadfile.
 *
 * Python signature: loadfile(filename)
 */
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

/**
 * new-function for Python object.
 *
 * Creates a new lua state and sets the memory limit.
 *
 * \param memory_limit The initial memory limit, in bytes or 0,
 *                     for no memory limit.
 *
 * Python signature: Sandbox(memory_limit=0)
 */
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

/**
 * Protected-mode lua function call.
 *
 * \see lua_pcall.
 *
 * Python signature: pcall(nargs, nresults, errfunc)
 */
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

/**
 * Pop top element from the stack.
 *
 * Pops the top element from the lua stack and returns it converted to an
 * appropriate Python value.
 *
 * \see lua_pop
 *
 * Python signature: pop()
 */
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

/**
 * Push a value on top of the lua stack.
 *
 * \see python_to_lua.
 *
 * Python signature: push(value)
 */
PyObject *Sandbox_push(Sandbox *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"value", NULL};

	PyObject *value;
	if (! PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &value)) {
		PyErr_SetString(PyExc_Exception, "Error parsing arguments.");
		return NULL;
	}

	if (! python_to_lua(self->L, value)) {
		/* Error string is set by python_to_loa. */
		return NULL;
	}

	/* all done */
	Py_RETURN_NONE;
}

/**
 * Setter for memory_limit (see lua_max_mem).
 */
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

/**
 * Getter/Setter struct.
 *
 * Currently, only contains memory_limit.
 */
static PyGetSetDef Sandbox_getseters[] = {
	{"memory_limit", (getter)Sandbox_getmemory_limit, (setter)Sandbox_setmemory_limit, "maximum allowed script memory usage (in bytes)", NULL},
	{NULL}
};

/**
 * Sandbox attributes.
 */
static PyMemberDef Sandbox_members[] = {
	{NULL}
};

/**
 * Sandbox methods.
 */
static PyMethodDef Sandbox_methods[] = {
	{"gettop", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_gettop, METH_NOARGS, "get number of elements in stack"},
	{"loadfile", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_loadfile, METH_KEYWORDS, "load a file"},
	{"loadstring", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_loadstring, METH_KEYWORDS, "load a string"},
	{"pcall", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_pcall, METH_KEYWORDS, "protected function call"},
	{"push", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_push, METH_KEYWORDS, "push value on top of lua stack"},
	{"pop", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_pop, METH_NOARGS, "pop and return"},
	{NULL}
};

/**
 * INIT-function for Sandbox type.
 */
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

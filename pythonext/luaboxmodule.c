#include <Python.h>
#include <structmember.h>

/* lua headers */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* note: the cast is wrong, as we really need a PyCFunctionWithKeywords,
 *       however the type in PyMethodDef is still PyCFunction we would
 *       like to suppress warnings */
#define SUPPRESS_PYMCFUNCTION_WARNINGS (PyCFunction) 

/* Exception type, used for all errors that can occur in this module. */
static PyObject *Exc_LuaBoxException;
static PyObject *Exc_OutOfMemory;
static PyObject *Exc_SyntaxError;

/* the sandbox type */
typedef struct {
	PyObject_HEAD
	size_t lua_max_mem;
	size_t lua_current_mem;
	lua_State *L;
	char *lua_error_msg;
} Sandbox;

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

/* destructor */
static void Sandbox_dealloc(Sandbox *self) {
	if (self->L) lua_close(self->L);
	if (self->lua_error_msg) free(self->lua_error_msg);
	self->ob_type->tp_free((PyObject*)self);
}

/* memory_limit (lua_max_mem) setter */
static PyObject *Sandbox_getmemory_limit(Sandbox *self, void *closure) {
	return Py_BuildValue("K", self->lua_max_mem);
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

/* new function */
static PyObject* Sandbox_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
	Sandbox *self = (Sandbox*) type->tp_alloc(type, 0);
	self->lua_error_msg = 0;

	if(self) {
		PyObject *memory_limit;
		static char *kwlist[] = {"memory_limit", NULL};
		if(! PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &memory_limit)) return NULL;

		/* memory_limit is zero if not supplied */
		if (! memory_limit) self->lua_max_mem = 0;
		if (-1 == Sandbox_setmemory_limit(self, memory_limit, NULL)) return NULL;

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

/* constructor */
static int Sandbox_init(Sandbox *self, PyObject *args, PyObject *kwds) {
	return 0;
}

static PyObject* Sandbox_dofile(PyObject *_self, PyObject *args, PyObject *kwds) {
	Sandbox *self = (Sandbox*) _self;
	const char *filename;
	static char *kwlist[] = {"filename", NULL};
	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &filename)) {
		PyErr_SetString(PyExc_Exception, "Error parsing arguments.");
		return NULL;
	}

	int rval;

	printf("Running filename %s\n", filename);
	rval = luaL_dofile(self->L, filename);
	printf("Got return value: %d\n", rval);

	return Py_BuildValue("I", rval);
}

static PyObject* Sandbox_pop(PyObject *_self, PyObject *args) {
	Sandbox *self = (Sandbox*) _self;

	if (0 == lua_gettop(self->L)) {
		PyErr_SetString(PyExc_IndexError, "Lua stack is empty.");
		return NULL;
	}

	if (! lua_isstring(self->L, -1)) {
		PyErr_SetString(PyExc_TypeError, "Top element of stack is not a string or string-convertable.");
		return NULL;
	}

	PyObject* rval = Py_BuildValue("s", lua_tostring(self->L, -1));
	lua_pop(self->L, 1);
	return rval;
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
	{"dofile", SUPPRESS_PYMCFUNCTION_WARNINGS Sandbox_dofile, METH_KEYWORDS, "see lua doc for details."},
	{"pop", Sandbox_pop, METH_NOARGS, "pop and return"},
	{NULL}
};

static PyTypeObject SandboxType = {
	PyObject_HEAD_INIT(NULL)
	0,                                  /*ob_size*/
	"luabox.Sandbox",                   /*tp_name*/
	sizeof(Sandbox)                    /*tp_basicsize*/

	/* other members initialized in module init */
};

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

	/* add sandbox type */
	SandboxType.tp_new = PyType_GenericNew;
	SandboxType.tp_dealloc = (destructor)Sandbox_dealloc;
	SandboxType.tp_flags = Py_TPFLAGS_DEFAULT;
	SandboxType.tp_doc = "Lua sandbox. Documentation sadly lacking.";
	SandboxType.tp_methods = Sandbox_methods;
	SandboxType.tp_members = Sandbox_members;
	SandboxType.tp_getset = Sandbox_getseters;
	SandboxType.tp_init = (initproc)Sandbox_init;
	SandboxType.tp_new = Sandbox_new;

	if(PyType_Ready(&SandboxType) < 0) return;

	Py_XINCREF(&SandboxType);
	PyModule_AddObject(m, "Sandbox", (PyObject*) &SandboxType);
}

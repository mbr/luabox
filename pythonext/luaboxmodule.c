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
static PyObject *LuaBoxError;

/* the sandbox type */
typedef struct {
	PyObject_HEAD
	size_t lua_max_mem;
	size_t lua_current_mem;
	lua_State *L;
} Sandbox;

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
	//else printf("No freeing, no interpreter!\n");
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
			PyErr_SetString(LuaBoxError, "Could not instantiate lua state, maybe increase the memory limit?");
			Py_XDECREF(self);
			return NULL;
		}
	}

	return (PyObject*)self;
}

/* constructor */
static int Sandbox_init(Sandbox *self, PyObject *args, PyObject *kwds) {
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

	/* set up exception type */
	LuaBoxError = PyErr_NewException("luabox.error", NULL, NULL);
	Py_XINCREF(LuaBoxError);
	PyModule_AddObject(m, "error", LuaBoxError);

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

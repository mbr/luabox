#include <Python.h>
#include <structmember.h>

/* Exception type, used for all errors that can occur in this module. */
static PyObject *LuaBoxError;

/* the sandbox type */
typedef struct {
	PyObject_HEAD
	size_t lua_max_mem;
} Sandbox;

/* destructor */
static void Sandbox_dealloc(Sandbox *self) {
	self->ob_type->tp_free((PyObject*)self);
}

/* new function */
static PyObject* Sandbox_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
	Sandbox *self;

	self = (Sandbox*) type->tp_alloc(type, 0);


	return (PyObject*)self;
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

/* constructor */
static int Sandbox_init(Sandbox *self, PyObject *args, PyObject *kwds) {
	PyObject *memory_limit;
	static char *kwlist[] = {"memory_limit", NULL};
	if(! PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &memory_limit)) return -1;

	/* memory_limit is zero if not supplied */
	if (! memory_limit) self->lua_max_mem = 0;
	if (-1 == Sandbox_setmemory_limit(self, memory_limit, NULL)) return -1;

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
	sizeof(Sandbox),                    /*tp_basicsize*/
	0,                                  /*tp_itemsize*/
	(destructor)Sandbox_dealloc,        /*tp_dealloc*/
	0,                                  /*tp_print*/
	0,                                  /*tp_getattr*/
	0,                                  /*tp_setattr*/
	0,                                  /*tp_compare*/
	0,                                  /*tp_repr*/
	0,                                  /*tp_as_number*/
	0,                                  /*tp_as_sequence*/
	0,                                  /*tp_as_mapping*/
	0,                                  /*tp_hash */
	0,                                  /*tp_call*/
	0,                                  /*tp_str*/
	0,                                  /*tp_getattro*/
	0,                                  /*tp_setattro*/
	0,                                  /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,                 /*tp_flags*/
	"Lua sandbox. Document me",         /* tp_doc */
	0,		                            /* tp_traverse */
	0,		                            /* tp_clear */
	0,		                            /* tp_richcompare */
	0,		                            /* tp_weaklistoffset */
	0,		                            /* tp_iter */
	0,		                            /* tp_iternext */
	Sandbox_methods,                    /* tp_methods */
	Sandbox_members,                    /* tp_members */
	Sandbox_getseters,                  /* tp_getset */
	0,                                  /* tp_base */
	0,                                  /* tp_dict */
	0,                                  /* tp_descr_get */
	0,                                  /* tp_descr_set */
	0,                                  /* tp_dictoffset */
	(initproc)Sandbox_init,             /* tp_init */
	0,                                  /* tp_alloc */
	Sandbox_new,                        /* tp_new */
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
	SandboxType.tp_new = PyType_GenericNew; /* could be done in definition, but this works on more compilers! */
	if(PyType_Ready(&SandboxType) < 0) return;

	Py_XINCREF(&SandboxType);
	PyModule_AddObject(m, "Sandbox", (PyObject*) &SandboxType);
}

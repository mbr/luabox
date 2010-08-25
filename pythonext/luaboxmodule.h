#ifndef LUABOXMODULE_H
#define LUABOXMODULE_H 

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
extern PyObject *Exc_LuaBoxException;

/* Exception subtypes */
extern PyObject *Exc_OutOfMemory;
extern PyObject *Exc_SyntaxError;
extern PyObject *Exc_RuntimeError;
extern PyObject *Exc_ErrorError;

/* from sandbox.c */
typedef struct {
	PyObject_HEAD
	size_t lua_max_mem;
	size_t lua_current_mem;
	lua_State *L;
	char *lua_error_msg;
} Sandbox;

typedef struct {
	PyObject_HEAD
	Sandbox *sandbox;
	int ref;
} LuaTableRef;

extern PyTypeObject SandboxType;
extern PyTypeObject LuaTableRefType;

void SandboxType_INIT(PyTypeObject *t);

/* from types.c */
PyObject *lua_to_python(lua_State *L);
int python_to_lua(lua_State *L, PyObject *obj);

/* from luatableref.c */
PyObject *LuaTableRef_from_stack(Sandbox *sandbox);
void LuaTableRefType_INIT(PyTypeObject *t);

#endif /* LUABOXMODULE_H */

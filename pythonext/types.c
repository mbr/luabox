#include "luaboxmodule.h"

/* converts the primite lua object on top of the stack to a python object. does not pop it from the stack */
PyObject *lua_to_python(lua_State *L) {
	const int index = -1;

	if (0 == lua_gettop(L)) {
		PyErr_SetString(PyExc_IndexError, "Lua stack is empty.");
		return NULL;
	}

	int t = lua_type(L, index);
	switch(t) {
		case LUA_TNIL:
			Py_RETURN_NONE;

		case LUA_TNUMBER:
			/* note: code assumes lua_Number is double */
			return Py_BuildValue("d", lua_tonumber(L, index));

		case LUA_TBOOLEAN:
			if (lua_toboolean(L, index)) Py_RETURN_TRUE;
			else Py_RETURN_FALSE;

		case LUA_TSTRING:
			return Py_BuildValue("s", lua_tostring(L, index));

		default:
			lua_typename(L, t);
			return PyErr_Format(PyExc_TypeError, "Don't know how to convert lua type '%s' to appropriate Python type.", lua_typename(L, t));
	}
}

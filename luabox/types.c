/**
 * Conversion between lua and Python types.
 */
#include "luaboxmodule.h"

/**
 * Converts the primitive lua object on top of the stack to a python object.
 *
 * Does not pop it from the stack.
 */
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

/**
 * Puts a new lua object on the stack that is a copy of the given Python
 * object.
 */
int python_to_lua(lua_State *L, PyObject *obj) {
	/* PyBool is a subtype of Int, so check first if it is a boolean. */
	if (PyBool_Check(obj)) {
		/* Boolean */
		if (Py_True == obj) {
			lua_pushboolean(L, 1);
		} else if (Py_False == obj) {
			lua_pushboolean(L, 0);
		} else {
			/* this should never be reached */
			PyErr_SetString(PyExc_TypeError, "These are not the booleans you're looking for. Seriously, wtf?");
		}
	} else if (PyInt_Check(obj)) {
		/* Integer */

		/* convert to double (lua_Number), then push */
		lua_pushnumber(L, (lua_Number) PyInt_AS_LONG(obj));
	} else if (PyLong_Check(obj)) {
		/* Long Integer */
		lua_pushnumber(L, (lua_Number) PyLong_AsDouble(obj));

		/* check for exception */
		if (PyErr_Occurred()) {
			lua_pop(L, 1); // pop error value from PyLong_AsDouble (== -1.0)
			return 0;
		}
	} else if (PyFloat_Check(obj)) {
		/* Float */
		lua_pushnumber(L, (lua_Number) PyFloat_AsDouble(obj));
	} else  if (PyString_Check(obj)) {
		/* String */
		lua_pushlstring(L, PyString_AS_STRING(obj), PyString_GET_SIZE(obj));
	} else if (Py_None == obj) {
		lua_pushnil(L);
	}
	else {
		PyErr_Format(PyExc_TypeError, "Don't know how to convert type '%s' to lua object.", obj->ob_type->tp_name);
		return 0;
	}

	return 1;
}

#include "luaboxmodule.h"

/* converts the top object on the stack to a python object. does not pop it from the stack */
PyObject *lua_to_python(lua_State *L) {
	const int index = -1;

	if (0 == lua_gettop(L)) {
		PyErr_SetString(PyExc_IndexError, "Lua stack is empty.");
		return NULL;
	}

	int t = lua_type(L, index);
	PyObject *d;
	switch(t) {
		case LUA_TNIL:
			Py_RETURN_NONE;

		case LUA_TNUMBER:
			/* note: code assumes lua_Number is double */
			return Py_BuildValue("d", lua_tonumber(L, index));

		case LUA_TBOOLEAN:
			if (lua_toboolean(L, index)) Py_RETURN_TRUE;
			else Py_RETURN_FALSE;

		case LUA_TTABLE:
			/* create new python dictionary */
			d = PyDict_New();

			/* create absolute index */
			int tbl = index;
			if (tbl < 0) tbl += lua_gettop(L)+1;

			/* push nil (first index).
			 * stack: t+1 nil
			 *        t table */
			lua_pushnil(L);

			while(0 != lua_next(L, tbl)) {
				/* stack now:
				 * -1: value
				 * -2: key
				 * ...
				 * t: table
				 */
				/* value on top, now recurse. */
				PyObject *key, *value;

				/* get value and pop, leaving key */
				value = lua_to_python(L);
				lua_pop(L, 1);

				key = lua_to_python(L);
				/* note: if the key is a dictionary, this will blow up
				 * as dicts are not hashable in python */
				/* does not pop key, used for stateless iterator */

				PyDict_SetItem(d, key, value);

				/* decrement reference counts, only hold items in dictionary */
				Py_DECREF(key);
				Py_DECREF(value);
			}

			/* stack is "clean", only table left at this point */
			return d;

		case LUA_TSTRING:
			return Py_BuildValue("s", lua_tostring(L, index));

		default:
			lua_typename(L, t);
			return PyErr_Format(PyExc_TypeError, "Don't know how to convert lua type '%s' to appropriate Python type.", lua_typename(L, t));
	}
}

#!/usr/bin/env python
# coding=utf8

from distutils.core import setup, Extension
import commands

# function below from http://code.activestate.com/recipes/502261-python-distutils-pkg-config/
def pkgconfig(*packages, **kw):
	flag_map = {'-I': 'include_dirs', '-L': 'library_dirs', '-l': 'libraries'}
	for token in commands.getoutput("pkg-config --libs --cflags %s" % ' '.join(packages)).split():
		kw.setdefault(flag_map.get(token[:2]), []).append(token[2:])
	return kw

mod = Extension('luabox',
                ['luabox/luaboxmodule.c',
                 'luabox/sandbox.c',
                 'luabox/types.c',
                 'luabox/luatableref.c'],
                **pkgconfig('lua5.1'))

setup(name = 'LuaBox',
      version = '0.1',
      description = 'Allows running sandbox instances of lua with memory and CPU usage bounds inside python to execute user-submitted code.',
      ext_modules = [mod])

#!/usr/bin/env python
# coding=utf8

from distutils.core import setup, Extension
mod = Extension('luabox',
                sources = ['luaboxmodule.c'])

setup(name = 'LuaBox',
      version = '0.1',
      description = 'Allows running sandbox instances of lua with memory and CPU usage bounds inside python to execute user-submitted code.',
      ext_modules = [mod])

#!/usr/bin/env python
# coding=utf8

import luabox

class LuaSandbox(luabox.Sandbox):
	def stack_as_tuple(self):
		l = []
		for i in range(0,self.gettop()):
			l.append(self.pop())
		l.reverse()
		return tuple(l)
	def run(self, *args):
		# push all arguments onto stack
		#print "NOT pushing args"
		#self.pcall(nargs = len(args))

		self.pcall(nargs = 0, nresults = luabox.LUA_MULTRET)
		return self.stack_as_tuple()

s = LuaSandbox()
#s.loadstring("a = {a = 'b', c = {x = 5}, [{three = four}] = 99, [2] = 1}; return a")
s.loadstring("a = {a = 'b', c = {x = 5}, x = 99, [2] = 1}; return a")
for v in s.run():
	print type(v),v

s.loadstring("a = {a = 'b', c = {x = 5}, x = 99, [2] = 1, [1] = 66}; return a")
tbl = s.run()[0]
print tbl

print len(tbl)

keys = [99, 1L, 2, "2", 'x', 'foo']

for k in keys:
	v = tbl[k]
	print "value of key %s %s: %s %s" % (type(k), k, type(v), v)

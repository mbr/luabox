#!/usr/bin/env lua

ptbl = {}

i = 0
j = 0
while i < 10000000 do
	print(i)
	ptbl[i] = i
	i = i+1
end

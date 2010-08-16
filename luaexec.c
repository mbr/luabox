#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

lua_State *L;

#define MEMLIMIT 1000000
void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
	size_t* memcounter = (size_t*) ud;
	void *nptr;

	size_t newmem_size = nsize-osize;
	if (*memcounter+newmem_size >= MEMLIMIT) {
		printf("Memory denied! Would-be size: %ld\n", *memcounter+newmem_size);
		return NULL;
	}
	if (nsize == 0) {
		free(ptr);
		nptr = NULL;
	} else {
		nptr = realloc(ptr, nsize);
	}
	*memcounter += newmem_size;
	//printf("l_alloc(%lx, %lx, %ld, %ld): %lx (new usage %ld)\n", (size_t)ud, (size_t)ptr, osize, nsize, (size_t)nptr, *memcounter);
	return nptr;
}

#define CYCLECOUNT 5000
void *l_chook(lua_State *L, lua_Debug *ar) {
	if (LUA_HOOKCOUNTT == ar->event) {
		printf("Cycle count: %d\n", CYCLECOUNT);
	}
}

int main(int argc, char **argv) {
	size_t memcount = 0;
	L = lua_newstate(l_alloc, &memcount);

	lua_setallocf(L, l_alloc, &memcount);

	luaopen_base(L);
	luaL_dofile(L, argv[1]);

	lua_close(L);

	return 0;
}

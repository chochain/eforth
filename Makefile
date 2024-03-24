EM = em++
CC = g++

all: tests/eforth
	$(CC) -Isrc -o $@ src/ceforth.cpp -O3

wasm: tests/eforth.js
	$(EM) -Isrc -o $@ src/ceforth.cpp -sEXPORTED_FUNCTIONS=_main,_forth -sEXPORTED_RUNTIME_METHODS=cwrap -O3
	cp tests/eforth.* ../weforth/tests



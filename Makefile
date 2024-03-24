EM = em++
CC = g++

all: tests/eforth tests/eforth.js

tests/eforth: src/ceforth.cpp
	$(CC) -Isrc -o $@ $< -O3

tests/eforth.js: src/ceforth.cpp
	$(EM) -Isrc -o $@ $< -sEXPORTED_FUNCTIONS=_main,_forth -sEXPORTED_RUNTIME_METHODS=cwrap -O3
	cp tests/eforth.* ../weforth/tests



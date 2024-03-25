EM = em++
CC = g++

410: tests/ceforth410

all: tests/eforth tests/eforth.js

tests/ceforth410: orig/ting/ceForth_410.cpp
	$(CC) -Isrc -o $@ $< -O3

tests/eforth: src/ceforth.cpp
	$(CC) -Isrc -o $@ $< -O3

tests/eforth.js: src/ceforth.cpp
	$(EM) -Isrc -o $@ $< -sEXPORTED_FUNCTIONS=_main,_forth -sEXPORTED_RUNTIME_METHODS=cwrap -O3
	cp tests/eforth.* ../weforth/tests



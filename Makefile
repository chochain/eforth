EM = em++
CC = g++

FLST = \
	tests/ceforth403 \
	tests/ceforth410 \
	tests/eforth     \
	tests/eforth.js  \
	tests/eforth.wasm

exe: tests/eforth

403: tests/ceforth403

410: tests/ceforth410

wasm: tests/eforth.js

all: exe 403 wasm

tests/eforth: src/ceforth.cpp
	$(CC) -Isrc -o $@ $< -O3

tests/ceforth403: orig/ting/ceForth_403.cpp
	$(CC) -Isrc -o $@ $< -O3

tests/ceforth410: orig/ting/ceForth_410.cpp
	$(CC) -Isrc -o $@ $< -O3

tests/eforth.js: src/ceforth.cpp
	$(EM) -Isrc -o $@ $< -sEXPORTED_FUNCTIONS=_main,_forth -sEXPORTED_RUNTIME_METHODS=cwrap -O3
	cp tests/eforth.* ../weforth/tests

clean:
	rm $(FLST)



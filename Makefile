EM = em++
CC = g++
CC_FLAG = \
	-std=c++11 -Winline -finline-functions \
	-std=c++17 -Wall -g3 -O3

FLST = \
	tests/ceforth36b \
	tests/ceforth40x \
	tests/eforth     \
	tests/eforth.js  \
	tests/eforth.wasm

exe: tests/eforth

36b: tests/ceforth36b

40x: tests/ceforth40x

wasm: tests/eforth.js

all: exe 36b 40x wasm

%.o: %.cpp
	$(CC) $(CC_FLAG) -Isrc -c -o $@ $<

tests/eforth: platform/main.o src/ceforth.o
	$(CC) -o $@ $^

tests/ceforth36b: orig/ting/ceforth_36b.cpp
	$(CC) $(CC_FLAG) -o $@ $<

tests/ceforth40x: platform/main.o orig/40x/ceforth.cpp
	$(CC) $(CC_FLAG) -Iorig/40x/src -o $@ $^

tests/eforth.js: src/ceforth.cpp
	$(EM) -Isrc -o $@ $< -sEXPORTED_FUNCTIONS=_main,_forth -sEXPORTED_RUNTIME_METHODS=cwrap -O3
	cp tests/eforth.* ../weforth/tests

clean:
	rm src/*.o platform/*.o $(FLST)



EM = em++
CC = g++
CC_FLAG = -std=c++17 -O2 -Wall \
          -fomit-frame-pointer -fno-stack-check -fno-stack-protector

FLST = \
	tests/ceforth36b  \
	tests/ceforth40x  \
	tests/eforth      \
	tests/eforth.html \
	tests/eforth.js   \
	tests/eforth.wasm

exe: tests/eforth

36b: tests/ceforth36b

40x: tests/ceforth40x

wasm: tests/eforth.html

all: exe 36b 40x wasm

%.o: %.cpp
	$(CC) $(CC_FLAG) -Isrc -c -o $@ $<

tests/eforth: platform/main.o src/ceforth.o src/ceforth_sys.o
	$(CC) -o $@ $^

tests/ceforth36b: orig/ting/ceforth_36b.cpp
	$(CC) $(CC_FLAG) -o $@ $<

tests/ceforth40x: platform/main.o orig/40x/ceforth.cpp
	$(CC) $(CC_FLAG) -Iorig/40x -o $@ $^

tests/eforth.html: platform/wasm.cpp src/ceforth.cpp
	$(EM) -Isrc -o $@ $^ \
	  --shell-file platform/eforth.html \
	  -sEXPORTED_FUNCTIONS=_main,_forth \
	  -sEXPORTED_RUNTIME_METHODS=cwrap -O2

clean:
	rm src/*.o platform/*.o $(FLST)



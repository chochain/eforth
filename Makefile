EM = em++
CC = g++
CC_FLAG = -std=c++17 -g -O2 -Wall \
          -fomit-frame-pointer -fno-stack-check -fno-stack-protector

FLST = \
	tests/ceforth50x  \
	tests/eforth      \
	tests/eforth.html \
	tests/eforth.js   \
	tests/eforth.wasm

exe: tests/eforth

50x: tests/ceforth50x

wasm: tests/eforth.html

all: exe 50x wasm

%.o: %.cpp
	$(CC) $(CC_FLAG) -Isrc -c -o $@ $<

tests/eforth: platform/main.o src/ceforth.o src/ceforth_sys.o src/ceforth_task.o
	$(CC) $(CC_FLAG) -pthread -o $@ $^

tests/ceforth50x: platform/main.o orig/50x/ceforth.o orig/50x/ceforth_sys.o orig/50x/ceforth_task.o
	$(CC) $(CC_FLAG) -pthread -o $@ $^

tests/eforth.html: platform/wasm.cpp src/ceforth.cpp src/ceforth_sys.cpp
	$(EM) -Isrc -o $@ $^ \
	  --shell-file platform/eforth.html \
	  -sEXPORTED_FUNCTIONS=_main,_forth \
	  -sEXPORTED_RUNTIME_METHODS=cwrap -O2

clean:
	rm orig/50x/*.o src/*.o platform/*.o $(FLST)



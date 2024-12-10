EM = em++
EM_FLAG = -std=c++17 -O0 \
          -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=2 \
          -s ENVIRONMENT='worker'
#-s PTHREAD_POOL_SIZE='navigator.hardwareConcurrency'

CC = g++
CC_FLAG = -std=c++17 -g -O3 -Wall -pthread \
          -fomit-frame-pointer -fno-stack-check -fno-stack-protector \
		  -march=native -ffast-math -funroll-loops

FLST = \
	tests/ceforth50x  \
	tests/eforth      \
	tests/eforth.html \
	tests/eforth.js   \
	tests/eforth.wasm

exe: tests/eforth

50x: tests/ceforth50x

wasm: tests/eforth.js

all: exe 50x wasm

%.o: %.cpp
	$(CC) $(CC_FLAG) -Isrc -c -o $@ $<

tests/eforth: platform/main.o src/ceforth.o src/ceforth_sys.o src/ceforth_task.o
	$(CC) $(CC_FLAG) -o $@ $^

tests/ceforth50x: platform/main.o orig/50x/ceforth.o orig/50x/ceforth_sys.o orig/50x/ceforth_task.o
	$(CC) $(CC_FLAG) -o $@ $^

#tests/eforth.js: platform/wasm.cpp src/ceforth.cpp src/ceforth_sys.cpp src/ceforth_task.cpp
tests/eforth.js: platform/wasm.cpp src/jk.cpp
	cp platform/eforth_vm0.js platform/eforth.html tests
	$(EM) $(EM_FLAG) -Isrc -o $@ $^ \
	  -sEXPORTED_FUNCTIONS=_main,_forth \
	  -sEXPORTED_RUNTIME_METHODS=cwrap

clean:
	rm orig/50x/*.o src/*.o platform/*.o $(FLST)



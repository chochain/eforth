EM = em++
EM_FLAG = -std=c++17 -O2 \
          -pthread -s USE_PTHREADS=1 \
          -s PTHREAD_POOL_SIZE='navigator.hardwareConcurrency'

CXX = g++
CXXFLAG = -std=gnu++17 -g -O2 -Wall -pthread \
          -fno-pie -no-pie -DXT0_U32=1       \
          -fomit-frame-pointer -fno-stack-check -fno-stack-protector \
		  -march=native -ffast-math -funroll-loops

CC = g++
CC_FLAG = -std=gnu++17 -g -O2 -Wall -pthread \
          -fno-pie -no-pie -DXT0_U32=1       \
          -fomit-frame-pointer -fno-stack-check -fno-stack-protector \
		  -march=native -ffast-math -funroll-loops

FLST = \
	tests/ceforth50x  \
	tests/eforth      \
	tests/eforth.html \
	tests/eforth.js   \
	tests/eforth.wasm

OBJS = \
	src/ceforth.o \
	src/ceforth_sys.o \
	src/ceforth_task.o

OBJS_50X = \
	orig/50x/ceforth.o \
	orig/50x/ceforth_sys.o \
	orig/50x/ceforth_task.o

OBJS_60X = \
	orig/60x/ceforth.o \
	orig/60x/ceforth_sys.o \
	orig/60x/ceforth_task.o

exe: tests/eforth

mqtt: tests/eforth_mqtt_dev tests/eforth_mqtt_ctl

50x: tests/ceforth50x

60x: tests/ceforth60x

wasm: tests/eforth.js

all: exe 60x wasm

%.o: %.cpp
	$(CXX) $(CXXFLAG) -Isrc -c -o $@ $<

%.o: %.c
	$(CC) $(CC_FLAG) -Isrc -c -o $@ $<

tests/eforth: platform/main.o $(OBJS)
	$(CC) $(CC_FLAG) -o $@ $^

tests/eforth_mqtt_dev: platform/mqtt.o platform/main_mqtt_dev.o $(OBJS)
	$(CC) -o $@ $^ -lpaho-mqtt3a

tests/eforth_mqtt_ctl: platform/mqtt.o platform/main_mqtt_ctl.o
	$(CC) -o $@ $^ -lpaho-mqtt3a

debug: tests/eforth
	/bin/valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $^

tests/ceforth50x: platform/main.o $(OBJS_50X)
	$(CC) $(CC_FLAG) -o $@ $^

tests/ceforth60x: platform/main.o $(OBJS_60X)
	$(CC) $(CC_FLAG) -o $@ $^

debug60: tests/ceforth60x
	/bin/valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $^

tests/eforth.js: platform/wasm.cpp src/ceforth.cpp src/ceforth_sys.cpp src/ceforth_task.cpp
	cp platform/eforth_vm0.js platform/eforth.html tests
	$(EM) $(EM_FLAG) -Isrc -o $@ $^ \
	  -sEXPORTED_FUNCTIONS=_main,_forth \
	  -sEXPORTED_RUNTIME_METHODS=cwrap

clean:
	rm platform/*.o $(FLST) $(OBJS_60X) $(OBJS) 



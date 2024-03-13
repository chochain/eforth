EM = em++
CC = g++

all: src/ceforth.cpp
	$(CC) -Isrc -o tests/eforth src/ceforth.cpp -O3

wasm: src/ceforth.cpp
	$(EM) -Isrc -o tests/eforth.html src/ceforth.cpp -O3
	cp tests/eforth.* ../weforth/tests



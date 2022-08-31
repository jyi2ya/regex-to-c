CFLAGS=-std=c11 -g -Wall -Wshadow -Wextra -fsanitize=address -O0
OBJ=src/xutils.o src/token.o src/regtree.o

all: regex-to-c

regex-to-c: $(OBJ) src/main.o
	mkdir -p target/
	$(CC) $(CFLAGS) $(OBJ) src/main.o -o target/regex-to-c

clean:
	rm -rf target/*
	rm -f src/*.o

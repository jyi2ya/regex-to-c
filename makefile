CFLAGS=-std=c99 -g -Wall -Wshadow -Wextra -fsanitize=address -O0
OBJ=src/xutils.o src/token.o src/main.o src/regtree.o

target/regex-to-c: $(OBJ)
	mkdir -p target/
	$(CC) $(CFLAGS) $(OBJ) -o target/regex-to-c

clean:
	rm -rf target/*

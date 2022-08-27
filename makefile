CFLAGS=-std=c99 -g -Wall -Wshadow -Wextra

target/regex-to-c: src/main.c
	mkdir -p target
	$(CC) $(CFLAGS) -o target/regex-to-c src/main.c

clean:
	rm -rf target/*

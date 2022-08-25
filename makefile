CFLAGS=-g -Wall -Wshadow -Wextra

target/reg2dfa: src/main.c
	mkdir -p target
	$(CC) $(CFLAGS) -o target/reg2dfa -O2 src/main.c

clean:
	rm -rf target/*

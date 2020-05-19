# Projekt: proj2 test
# Autor: Krystof Halmo
# Datum: 24.4.2018


CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror

proj2: proj2.c
	$(CC) $(CFLAGS) proj2.c -o proj2 -lpthread -lrt
clean:
	rm -f proj2 proj2.o output
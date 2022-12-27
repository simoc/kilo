kilo: kilo.c
	$(CC) kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

all: kilo

clean:
	/bin/rm -f kilo

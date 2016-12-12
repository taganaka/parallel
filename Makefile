CC=gcc
CFLAGS=-Wall -pedantic -O2
CLIBS=
OUT=parallel
OBJS=parallel.o


$(OUT): parallel.o
	$(CC) $(CFLAGS) $(CLIBS) -o $(OUT) $(OBJS)
parallel.o: parallel.c
	$(CC) $(CFLAGS) -c parallel.c -o parallel.o

.PHONY: clean install

clean:
	rm -rf *.o $(OUT)

install: parallel
	install parallel /usr/local/bin/

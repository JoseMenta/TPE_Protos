include ../Makefile.inc

SOURCES = $(wildcard *.c) $(wildcard **/*.c)

all:
	$(COMPILER) $(CFLAGS) -o $(SERVER_NAME) $(SOURCES)

clean:
	rm -f *.o
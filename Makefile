CC = gcc
CFLAGS = -Wall -pedantic

all:
	$(CC) $(CFLAGS) gbx-reader-writer.c -o gbx-reader-writer

clean:
	rm -rf gbx-reader-writer

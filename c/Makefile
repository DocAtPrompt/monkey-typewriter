CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
LDFLAGS ?=

all: travesty travesty_hash

travesty: travesty.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

travesty_hash: travesty_hash.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f travesty travesty_hash

.PHONY: all clean

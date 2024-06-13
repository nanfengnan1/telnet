CC=$(cross_compile)gcc
CFLAGS=-g -Wall
PREFIX=/usr/local

.PHONY: all

all: telnet_client

telnet_client: telnet_client.c 
	$(CC) $(CFLAGS) $^ -o $@

install:
	install -m 755 telnet_client $(PREFIX)/bin
	install -m 766 telnet.h $(PREFIX)/include


uninstall:
	rm -fr $(PREFIX)/bin/telnet_client
	rm -fr $(PREFIX)/include/telnet.h	
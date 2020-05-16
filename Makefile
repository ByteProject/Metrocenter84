# Makefile
SHELL := /bin/bash
CC = gcc
CFLAGS = -O2 -ansi
V = 0

PREFIX = /usr
MANDIR = $(PREFIX)/share/man/man6
LIB = $(PREFIX)/share/retroinform
MAIN = inform

# collect all source .c files
SRC = $(wildcard src/*.c)
# change suffixes from .c to .o
OBJ = $(SRC:.c=.o)

.c.o:
ifeq ($V, 0)
	@echo "  CC      " $*.o
	@$(CC) $(CFLAGS) -c $*.c -o $*.o
else
	$(CC) $(CFLAGS) -c $*.c -o $*.o
endif

$(MAIN): $(OBJ)
ifeq ($V, 0)
	@echo "  CCLD    " $@
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
else
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
endif
	@strip $(MAIN)

.PHONY: clean
clean:
	rm -f $(OBJ) $(MAIN)

.PHONY: install
install:
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(MANDIR)
	install -d $(DESTDIR)$(LIB)
	install -d $(DESTDIR)$(LIB)/{bin,demos,metro84,tutor}
	install -c -m 755 $(MAIN) $(DESTDIR)$(PREFIX)/bin
	install -c -m 755 bin/i6 $(DESTDIR)$(PREFIX)/bin
	install -c -m 644 bin/a8.bin $(DESTDIR)$(LIB)/bin
	install -c -m 644 demos/cloak.inf $(DESTDIR)$(LIB)/demos
	install -c -m 644 demos/scenery.inf $(DESTDIR)$(LIB)/demos
	install -c -m 644 demos/shell.inf $(DESTDIR)$(LIB)/demos
	install -c -m 644 metro84/* $(DESTDIR)$(LIB)/metro84
	install -c -m 644 tutor/* $(DESTDIR)$(LIB)/tutor
	install -c -m 644 i6.6 $(DESTDIR)$(MANDIR)
	gzip $(DESTDIR)$(MANDIR)/i6.6
	ln -s $(LIB)/metro84/grammar.h $(DESTDIR)$(LIB)/metro84/Grammar.h
	ln -s $(LIB)/metro84/parser.h $(DESTDIR)$(LIB)/metro84/Parser.h
	ln -s $(LIB)/metro84/verblib.h $(DESTDIR)$(LIB)/metro84/VerbLib.h
	ln -s $(LIB)/metro84/scenery.h $(DESTDIR)$(LIB)/metro84/Scenery.h

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(MAIN)
	rm -f $(DESTDIR)$(PREFIX)/bin/i6
	rm -f $(DESTDIR)$(MANDIR)/i6.6.gz
	rm -rf $(DESTDIR)$(LIB)

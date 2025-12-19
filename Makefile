CC=gcc
ALL_FLAGS=-Wall -Wextra -Wshadow -pedantic -Wno-variadic-macros -Wnull-dereference -Wformat=2 -Wno-format-y2k
# CFLAGS=-O2 $(ALL_FLAGS)
CFLAGS=-ggdb $(ALL_FLAGS)
LDFLAGS=
#

all: xscreensaver-suspend

xscreensaver-suspend: xscreensaver-suspend.c

clean:
	rm xscreensaver-suspend

strip:
	strip -p xscreensaver-suspend


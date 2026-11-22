CC     ?= cc
CFLAGS  = -std=c11 -Wall -Wextra -O2
LDLIBS  = -lncursesw -ldl

# The version is read from the VERSION file and compiled into nnano (nnano --version).
VERSION := $(strip $(shell cat VERSION))
CPPFLAGS += -DNNANO_VERSION='"$(VERSION)"'
# System-wide plugins are looked for in PLUGINDIR (empty: none). install.sh sets it.
PREFIX    ?= /usr/local
PLUGINDIR ?= $(PREFIX)/lib/neonano/plugins
CPPFLAGS  += -DNNANO_PLUGIN_SYSDIR='"$(PLUGINDIR)"'

SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)
HDR     = $(wildcard src/*.h)

all: nnano

nnano: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

# Every object depends on VERSION, so a version bump rebuilds everything.
$(OBJ): VERSION

src/%.o: src/%.c $(HDR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f nnano src/*.o

.PHONY: all clean

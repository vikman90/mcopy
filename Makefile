# Fast file copying
# Based on memory-mapped files
# Victor Manuel Fernandez-Castro
# 20 March 2016

TARGET  ?= mcp
BIN     ?= /usr/local/bin
CC      ?= gcc
RM      ?= rm -f
CFLAGS  += -Wall -Wextra -pipe -O2
INSTALL ?= install -m 755

.PHONY: all build clean install uninstall test-root

all: build

build: $(TARGET)

clean:
	$(RM) $(TARGET)

install: test-root build
	$(INSTALL) $(TARGET) $(BIN)/$(TARGET)

uninstall: test-root
	$(RM) $(BIN)/$(TARGET)

test-root:
ifneq (${USER},root)
	$(error You must be root)
endif

$(TARGET): copy.c
	$(CC) $(CFLAGS) -o $@ $^

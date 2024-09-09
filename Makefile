FILE := Makefile
OS_EXT =
ifeq ($(OS),Windows_NT)
	OS_EXT := .win32
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
		OS_EXT := .linux
    endif
    ifeq ($(UNAME_S),Darwin)
		OS_EXT := .mac
    endif
endif

all:
	make --no-print-directory -f $(FILE)$(OS_EXT)

re:
	make --no-print-directory -f $(FILE)$(OS_EXT) re

clean:
	make --no-print-directory -f $(FILE)$(OS_EXT) clean

.PHONY: all re clean

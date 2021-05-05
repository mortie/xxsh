VERSION := 0.1
XXSH_VERSION := "\"$(VERSION) $(shell git rev-parse --short HEAD)\""

STATIC ?= 1

CFLAGS += \
	-Ilinenoise -DXXSH_VERSION=$(XXSH_VERSION) \
	-Wall -Wextra -Wno-unused-parameter -Wpedantic

ifeq ($(STATIC),1)
	LDFLAGS += -static
endif

xxsh: xxsh.c linenoise/linenoise.c

.PHONY: clean
clean:
	rm -f xxsh

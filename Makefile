VERSION := 0.1
XXSH_VERSION := "\"$(VERSION) $(shell git rev-parse --short HEAD)\""

CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wpedantic -DXXSH_VERSION=$(XXSH_VERSION)
xxsh: xxsh.c

.PHONY: clean
clean:
	rm -f xxsh

CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wpedantic
xxsh: xxsh.c

.PHONY: clean
clean:
	rm -f xxsh

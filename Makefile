VERSION := 0.1
XXSH_VERSION := $(VERSION) $(shell git rev-parse --short HEAD)

STATIC ?= 1

CFLAGS += \
	-Ithirdparty -DXXSH_VERSION="\"$(XXSH_VERSION)\"" \
	-D_LARGEFILE64_SOURCE \
	-Wall -Wextra -Wno-unused-parameter -Wpedantic

ifeq ($(STATIC),1)
	LDFLAGS += -static
endif

SRCS = xxsh.c \
	thirdparty/linenoise/linenoise.c \
	thirdparty/miniz/miniz.c \
	thirdparty/miniz/miniz_tdef.c \
	thirdparty/miniz/miniz_tinfl.c \
	thirdparty/miniz/miniz_zip.c

xxsh: $(SRCS)

Initrd: xxsh
	rm -rf Initrd.d
	mkdir Initrd.d
	cp xxsh Initrd.d/init
	echo init | cpio -D Initrd.d -ov | gzip >$@
	rm -rf Initrd.d

UINITRD_ARCH ?= invalid
uInitrd: Initrd
	mkimage -n 'XXSH $(XXSH_VERSION)' -A $(UINITRD_ARCH) -O linux -T ramdisk -C gzip -d Initrd uInitrd

.PHONY: clean
clean:
	rm -f xxsh Initrd uInitrd Initrd.d

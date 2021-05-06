# XXSH: The extra small, self-contained shell

XXSH is a single, self-contained binary with no dependencies.
It has no requirement about filesystem layout of any kind;
just somehow get the binary to execute and you're on your way.

There are situations where it's easy to load an ELF into memory and execute it,
but hard to prepare a whole filesystem with coreutils binaries in /bin
(or symlinks, in the case of busybox). In those situations, XXSH provides a way
to manually inspect and play with the system.

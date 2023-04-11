# XXSH: The extra small, self-contained shell

XXSH is a single, self-contained binary with no dependencies.
It has no requirement about filesystem layout of any kind;
just somehow get the binary to execute and you're on your way.

There are situations where it's easy to load an ELF into memory and execute it,
but hard to prepare a whole filesystem with coreutils binaries in /bin
(or symlinks, in the case of busybox). In those situations, XXSH provides a way
to manually inspect and play with the system.

[linenoise](https://github.com/antirez/linenoise) is used for the prompt,
and it supports basic line editing. It also features tab completion for file paths.

## Usage

Compile with `make`, run with `./xxsh`.

By default, `xxsh` is statically linked. To disable static linking,
build with `make STATIC=0`.

To cross compile, overwrite the `CC` variable. For example,
`make CC=aarch64-linux-gnu-gcc` will compile for 64-bit ARM.

To make uBoot uInitrd with xxsh as the '/init' file, pass in `UINITRD_ARCH` to the uInitrd target.
For example, `make CC=aarch64-linux-gnu-gcc UINITRD_ARCH=arm64 uInitrd` makes a uInitrd for arm64
with an xxsh compiled using aarch64-linux-gnu-gcc.

### Commands

The built-in commands are:

* `echo <strings...>`: Print space-separated strings to the screen.
* `ls [paths...]`: List the contents of directories.
* `stat [paths...]`: Show file status (permissions and owners).
* `pwd`: Print the current working directory.
* `cat [paths...]`: Print the contents of files.
* `zcat [paths...]`: Print the content of gzipped files.
* `cd <path>`: Change working directory.
* `env`: Print environment variables.
* `get <names...>`: Print the content of environment variables.
* `set <<name> <value>...>`: Set environment variables.
* `unset <names...>`: Unset environment variables.
* `rm <paths...>`: Delete files.
* `rmdir <paths...>`: Delete directories.
* `mkdir <paths...>`: Make directories.
* (Linux) `mount <source> <target> [type] [flags] [data]`: Mount a filesystem.
  'flags' is a comma-separated list of flags.
* (macOS) `mount <target> [type] [flags] [data]`: Mount a filesystem.
  'flags' is a comma-separated list of flags.
* `reboot`: Reboot the machine.
* `uname`: Get system information.
* `help`: Show help output.
* `exit`: Exit XXSH.

Anything else will be executed from the filesystem. For example,
`/bin/ls /foo` will use the program at `/bin/ls` to list the contents of `/foo`,
and `git status` will execute the program `git` from your `$PATH`.

The output of a command can be redirected to a file using the
redirection operator `>`:

* `ls > dircontent.txt`
* `>name uname`

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <libgen.h>

#include "linenoise/linenoise.h"

#ifndef XXSH_VERSION
#define XXSH_VERSION "version unknown"
#endif

#define commands \
	X(echo, "  Echo text") \
	X(ls, "    List files in a directory") \
	X(stat, "  Get info about a file") \
	X(pwd, "   Print the current working directory") \
	X(cat, "   See the content of files") \
	X(cd, "    Change directory") \
	X(env, "   List all environment variables") \
	X(get, "   Get environment variables") \
	X(set, "   Set environment variables") \
	X(unset, " Unset environment variables") \
	X(rm, "    Remove files") \
	X(rmdir, " Remove a directory") \
	X(mkdir, " Make a directory") \
	X(mount, " Mount a filesystem") \
	X(umount, "Unmount a mount point") \
	X(reboot, "Reboot the system") \
	X(uname, " Show system information") \
	X(help, "  Show this help text") \
	X(exit, "  Exit XXSH")

extern char **environ;

static int running = 1;
static FILE *outf;

static void readstr(char *line, size_t *start, size_t *end) {
	char *s = line;
	while (*line == ' ' || *line == '\t') line += 1;
	*start = line - s;

	while (*line != ' ' && *line != '\t' && *line != '\n' && *line != '\0') line += 1;
	*end = line - s;
}

static char *readarg(char **line) {
	char *arg;
	if (**line == '\0') {
		arg = *line;
		return NULL;
	}

	size_t start, end;
	readstr(*line, &start, &end);
	arg = *line + start;
	*line += end;
	if (**line != '\0') {
		**line = '\0';
		*line += 1;
	}

	if (*arg == '\0') {
		return NULL;
	} else {
		return arg;
	}
}

static int do_echo(char **line) {
	char *arg;
	int first = 1;
	while ((arg = readarg(line))) {
		if (first) {
			fprintf(outf, "%s", arg);
			first = 0;
		} else {
			fprintf(outf, " %s", arg);
		}
	}

	putc('\n', outf);
	return 0;
}

static int ls_path(char *path) {
	struct dirent **ents;
	int n = scandir(path, &ents, NULL, alphasort);
	if (n < 0) {
		perror(path);
		return -1;
	}

	size_t linelen = 0;
	for (int i = 0; i < n; ++i) {
		size_t namelen = strlen(ents[i]->d_name);
		if (linelen != 0 && linelen + namelen > 80) {
			fprintf(outf, "\n");
			linelen = 0;
		}

		if (linelen != 0) {
			fprintf(outf, "  ");
			linelen += 2;
		}

		if (ents[i]->d_type == DT_DIR) {
			fprintf(outf, "%s/", ents[i]->d_name);
			linelen += namelen + 1;
		} else {
			fprintf(outf, "%s", ents[i]->d_name);
			linelen += namelen;
		}
	}
	fprintf(outf, "\n");

	free(ents);
	return 0;
}

static int do_ls(char **line) {
	char *arg = readarg(line);
	if (!arg) {
		return ls_path(".");
	}

	char *arg2 = readarg(line);
	if (!arg2) {
		return ls_path(arg);
	}

	fprintf(outf, "%s:\n", arg);
	int ret;
	if ((ret = ls_path(arg)) < 0) {
		return ret;
	}

	do {
		fprintf(outf, "%s\n", arg2);
		if ((ret = ls_path(arg2)) < 0) {
			return ret;
		}

		arg2 = readarg(line);
	} while (arg2);

	return 0;
}

static int do_stat(char **line) {
	char *arg;
	while ((arg = readarg(line))) {
		struct stat st;
		if (stat(arg, &st) < 0) {
			perror(arg);
			return -1;
		}

		char uname[1024];
		struct passwd *pw;
		if ((pw = getpwuid(st.st_uid)) == NULL) {
			snprintf(uname, sizeof(uname), "%i", st.st_uid);
		} else {
			strncpy(uname, pw->pw_name, sizeof(uname));
		}

		char gname[1024];
		struct group *gr;
		if ((gr = getgrgid(st.st_gid)) == NULL) {
			snprintf(gname, sizeof(gname), "%i", st.st_gid);
		} else {
			strncpy(gname, gr->gr_name, sizeof(gname));
		}

		fprintf(outf, "%s: %o: %s:%s\n", arg, st.st_mode, uname, gname);
	}

	return 0;
}

static int do_pwd(char **line) {
	char path[4096];
	errno = 0;
	if (getcwd(path, sizeof(path)) == NULL) {
		perror("getcwd");
		return -1;
	}

	fprintf(outf, "%s\n", path);
	return 0;
}

static int do_cat(char **line) {
	char *arg = readarg(line);
	if (!arg) {
		return 0;
	}

	char buf[4096];

	do {
		FILE *f = fopen(arg, "r");
		if (f == NULL) {
			perror(arg);
			return -1;
		}

		while (1) {
			size_t n = fread(buf, 1, sizeof(buf), f);
			if (n == 0) {
				break;
			}

			fwrite(buf, 1, n, outf);
		}

		fflush(outf);
		if (fclose(f) < 0) {
			perror("fclose");
		}
	} while ((arg = readarg(line)));

	return 0;
}

static int do_cd(char **line) {
	char *arg = readarg(line);
	if (arg) {
		if (chdir(arg) < 0) {
			perror(arg);
			return -1;
		}
	} else {
		if (chdir("/") < 0) {
			perror("/");
			return -1;
		}
	}

	return 0;
}

static int do_env(char **line) {
	for (size_t i = 0; environ[i]; ++i) {
		fprintf(stderr, "%s\n", environ[i]);
	}

	return 0;
}

static int do_get(char **line) {
	int ret = 0;
	char *arg;
	while ((arg = readarg(line))) {
		char *env = getenv(arg);
		if (!env) {
			fprintf(stderr, "No such env var: %s\n", arg);
			ret = -1;
		} else {
			fprintf(outf, "%s\n", env);
		}
	}

	return ret;
}

static int do_set(char **line) {
	int ret = 0;
	while (1) {
		char *key = readarg(line);
		if (key == NULL) {
			break;
		}

		char *val = readarg(line);
		if (val == NULL) {
			fprintf(stderr, "Key without a value: %s\n", key);
			ret = -1;
			break;
		}

		if (setenv(key, val, 1) < 0) {
			fprintf(stderr, "setenv %s: %s\n", key, strerror(errno));
			ret = -1;
		}
	}

	return ret;
}

static int do_unset(char **line) {
	int ret = 0;
	char *arg;
	while ((arg = readarg(line))) {
		if (unsetenv(arg) < 0) {
			fprintf(stderr, "unsetenv %s: %s\n", arg, strerror(errno));
			ret = -1;
		}
	}

	return ret;
}

static int do_rm(char **line) {
	int ret = 0;
	char *arg;
	while ((arg = readarg(line))) {
		if (unlink(arg) < 0) {
			perror(arg);
			ret = -1;
		}
	}

	return ret;
}

static int do_rmdir(char **line) {
	int ret = 0;
	char *arg;
	while ((arg = readarg(line))) {
		if (rmdir(arg) < 0) {
			perror(arg);
			ret = -1;
		}
	}

	return ret;
}

static int do_mkdir(char **line) {
	int ret = 0;
	char *arg;
	while ((arg = readarg(line))) {
		if (mkdir(arg, 0777) < 0) {
			perror(arg);
			ret = -1;
		}
	}

	return ret;
}

static int do_mount(char **line) {
	char *source = readarg(line);
	char *target = readarg(line);
	char *fstype = readarg(line);
	char *flags = readarg(line);
	char *data = readarg(line);

	if (source == NULL || target == NULL) {
		fprintf(stderr, "Mount requires 2 arguments\n");
		return -1;
	}

	unsigned long mountflags = 0;
	size_t start = 0;
	if (flags) {
		for (size_t i = 0; ; ++i) {
			if (flags[i] == ',' || flags[i] == '\0') {
				flags[i] = '\0';
				char *str = flags + start;

				if (strcmp(str, "remount") == 0) {
					mountflags |= MS_REMOUNT;
				} else if (strcmp(str, "bind") == 0) {
					mountflags |= MS_BIND;
				} else if (strcmp(str, "shared") == 0) {
					mountflags |= MS_SHARED;
				} else if (strcmp(str, "private") == 0) {
					mountflags |= MS_PRIVATE;
				} else if (strcmp(str, "slave") == 0) {
					mountflags |= MS_SLAVE;
				} else if (strcmp(str, "unbindable") == 0) {
					mountflags |= MS_UNBINDABLE;
				} else if (strcmp(str, "move") == 0) {
					mountflags |= MS_MOVE;
#ifdef MS_DIRSYNC
				} else if (strcmp(str, "dirsync") == 0) {
					mountflags |= MS_DIRSYNC;
#endif
#ifdef MS_LAZYTIME
				} else if (strcmp(str, "lazytime") == 0) {
					mountflags |= MS_LAZYTIME;
#endif
				} else if (strcmp(str, "mandlock") == 0) {
					mountflags |= MS_MANDLOCK;
				} else if (strcmp(str, "noatime") == 0) {
					mountflags |= MS_NOATIME;
				} else if (strcmp(str, "noexec") == 0) {
					mountflags |= MS_NOEXEC;
				} else if (strcmp(str, "nosuid") == 0) {
					mountflags |= MS_NOSUID;
				} else if (strcmp(str, "rdonly") == 0) {
					mountflags |= MS_RDONLY;
#ifdef MS_REC
				} else if (strcmp(str, "rec") == 0) {
					mountflags |= MS_REC;
#endif
#ifdef MS_RELATIME
				} else if (strcmp(str, "relatime") == 0) {
					mountflags |= MS_RELATIME;
#endif
#ifdef MS_SILENT
				} else if (strcmp(str, "silent") == 0) {
					mountflags |= MS_SILENT;
#endif
#ifdef MS_STRICTATIME
				} else if (strcmp(str, "strictatime") == 0) {
					mountflags |= MS_STRICTATIME;
#endif
				} else if (strcmp(str, "synchronous") == 0) {
					mountflags |= MS_SYNCHRONOUS;
#ifdef MS_NOSYMFOLLOW
				} else if (strcmp(str, "nosymfollow") == 0) {
					mountflags |= MS_NOSYMFOLLOW;
#endif
				} else {
					fprintf(stderr, "Unknown mount option: '%s'\n", str);
					return -1;
				}

				if (flags[i] == '\0') {
					break;
				}
				start = i + 1;
			}
		}
	}

	if (mount(source, target, fstype, mountflags, data) < 0) {
		perror("mount");
		return -1;
	}

	return 0;
}

static int do_umount(char **line) {
	int ret = 0;
	char *arg;
	while ((arg = readarg(line))) {
		if (umount(arg) < 0) {
			perror(arg);
			ret = -1;
		}
	}

	return ret;
}

static int do_reboot(char **line) {
	if (reboot(RB_AUTOBOOT) < 0) {
		perror("reboot");
		return -1;
	}

	return 0;
}

static int do_uname(char **name) {
	struct utsname uts;
	if (uname(&uts) < 0) {
		perror("uname");
		return -1;
	}

	fprintf(outf, "%s %s %s %s %s\n",
			uts.sysname, uts.nodename, uts.release, uts.version, uts.machine);
	return 0;
}

static int do_help(char **line) {
#define X(name, desc) fprintf(outf, "%s: %s\n", #name, desc);
	commands
#undef X

	return 0;
}

static int do_exit(char **line) {
	running = 0;
	return 0;
}

static int do_exec(char *argv0, char **line) {
	pid_t pid = fork();
	if (pid == 0) {
		char *argv[1024];
		argv[0] = argv0;
		size_t i;
		for (i = 1; i < sizeof(argv) / sizeof(*argv) - 1; ++i) {
			argv[i] = readarg(line);
			if (argv[i] == NULL) {
				break;
			}
		}

		argv[i] = NULL;

		if (outf != stdout) {
			if (dup2(fileno(outf), fileno(stdout)) < 0) {
				perror("dup2");
				exit(1);
			}
		}

		if (execvp(argv0, argv) < 0) {
			perror(argv0);
			exit(1);
		}

		abort(); // We should never get here
	}

	int wstatus;
	if (waitpid(pid, &wstatus, 0) < 0) {
		perror("waitpid");
		return -1;
	}

	if (WIFEXITED(wstatus)) {
		return -WEXITSTATUS(wstatus);
	} else {
		return -128 - WTERMSIG(wstatus);
	}
}

static int run(char *line) {
	char redirpath[1024];
	int redirect = 0;
	int retval = 0;

	FILE *redirf = NULL;
	FILE *oldoutf = outf;

	size_t linelen = strlen(line);
	for (size_t i = 0; i < linelen; ++i) {
		if (line[i] == '>') {
			size_t start = 0;
			size_t end = 0;
			readstr(line + i + 1, &start, &end);
			start += i + 1;
			end += i + 1;

			if (line[start] == '\0') {
				fprintf(stderr, "Redirect with no argument\n");
				retval = -1;
				goto exit;
			}

			if (end - start >= sizeof(redirpath) - 1) {
				fprintf(stderr, "Too long redirection argument!\n");
				retval = -1;
				goto exit;
			}

			memcpy(redirpath, line + start, end - start);
			redirpath[end - start] = '\0';

			size_t removefrom = i;
			size_t removeto = end;

			memmove(line + removefrom, line + removeto + 1, linelen - removeto + 1);
			redirect = 1;
			break;
		}
	}

	if (redirect) {
		redirf = fopen(redirpath, "w");
		if (redirf == NULL) {
			perror(redirpath);
			retval = -1;
			goto exit;
		}

		fflush(outf);
		outf = redirf;
	}

	char *command = readarg(&line);
	if (!command) {
		goto exit;
	}

#define X(name, desc) \
		if (strcmp(command, #name) == 0) { \
			retval = do_ ## name(&line); \
			goto exit; \
		}
	commands
#undef X

	// Fall back to exec'ing a path
	retval = do_exec(command, &line);
	goto exit;

exit:
	if (redirf != NULL) {
		fclose(redirf);
		outf = oldoutf;
	}

	return retval;
}

// I kind of hate this whole function, but it works...
// TODO: make this less bad?
static void completion(const char *buf, linenoiseCompletions *lc) {
	size_t buflen = strlen(buf);

	int is_empty = 1;
	for (size_t i = 0; i < buflen; ++i) {
		if (buf[i] != ' ') {
			is_empty = 0;
			break;
		}
	}

	if (is_empty) {
#define X(name, desc) \
		linenoiseAddCompletion(lc, #name);
		commands
#undef X
		return;
	}

	if (buf[buflen - 1] == ' ') {
		struct dirent **names;
		int n = scandir(".", &names, NULL, alphasort);
		if (n < 0) {
			return;
		}

		char buffer[1024];
		for (int i = 0; i < n; ++i) {
			if (names[i]->d_name[0] != '.') {
				snprintf(buffer, sizeof(buffer), "%s./%s", buf, names[i]->d_name);
				linenoiseAddCompletion(lc, buffer);
			}
		}

		free(names);
		return;
	}

	char linebuf[1024];
	snprintf(linebuf, sizeof(linebuf), "%s", buf);

	char *arg = NULL;
	char *tmp;
	char *line = linebuf;
	while ((tmp = readarg(&line))) {
		arg = tmp;
	}

	if (arg == NULL) {
		return;
	}

	size_t arglen = strlen(arg);
	if (arg[arglen - 1] == '/') {
		struct dirent **names;
		int n = scandir(arg, &names, NULL, alphasort);
		if (n < 0) {
			return;
		}

		char buffer[1024];
		for (int i = 0; i < n; ++i) {
			if (names[i]->d_name[0] != '.') {
				snprintf(buffer, sizeof(buffer), "%s%s", buf, names[i]->d_name);
				linenoiseAddCompletion(lc, buffer);
			}
		}

		free(names);
		return;
	}

	// dirname and basename modify their contents, so create a new buffer
	// for dirname to mess up and let basename mess up the old one
	char argbuf[1024];
	snprintf(argbuf, sizeof(argbuf), "%s", arg);
	char *dirpart = dirname(argbuf);
	char *basepart = basename(arg);

	struct dirent **names;
	int n = scandir(dirpart, &names, NULL, alphasort);
	if (n < 0) {
		return;
	}

	char buffer[1024];
	for (int i = 0; i < n; ++i) {
		if (strncmp(names[i]->d_name, basepart, strlen(basepart)) == 0) {
			snprintf(
					buffer, sizeof(buffer), "%.*s%s",
					(int)(buflen - strlen(basepart)), buf, names[i]->d_name);
			linenoiseAddCompletion(lc, buffer);
		}
	}
}

int main() {
	outf = stdout;

	fprintf(outf, "XXSH %s\n", XXSH_VERSION);
	linenoiseHistorySetMaxLen(64);
	linenoiseSetCompletionCallback(completion);

	int ret = 0;
	while (running) {
		errno = 0;
		char *line = linenoise(">> ");
		if (line == NULL && errno == EAGAIN) { // CTRL + C
			continue;
		} else if (line == NULL) {
			break;
		}

		linenoiseHistoryAdd(line);

		int ret;
		ret = run(line);
		linenoiseFree(line);
		if (ret < 0) {
			fprintf(stderr, ":( %i\n", ret);
		}
	}

	fprintf(outf, "Exit.\n");
	return ret;
}

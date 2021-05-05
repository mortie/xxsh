#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <signal.h>

#include "linenoise/linenoise.h"

#ifndef XXSH_VERSION
#define XXSH_VERSION "version unknown"
#endif

#define commands \
	X(echo, " Echo text") \
	X(ls, "   List files in a directory") \
	X(stat, " Get info about a file") \
	X(pwd, "  Print the current working directory") \
	X(cat, "  See the content of files") \
	X(cd, "   Change directory") \
	X(rm, "   Remove files") \
	X(rmdir, "Remove a directory") \
	X(mkdir, "Make a directory") \
	X(mount, "Mount a filesystem") \
	X(help, " Show this help text") \
	X(exit, " Exit XXSH")

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
	struct dirent **names;
	int n = scandir(path, &names, NULL, alphasort);
	if (n < 0) {
		perror(path);
		return -1;
	}

	size_t linelen = 0;
	for (int i = 0; i < n; ++i) {
		size_t namelen = strlen(names[i]->d_name);
		if (linelen != 0 && linelen + namelen > 80) {
			fprintf(outf, "\n");
			linelen = 0;
		}

		if (linelen == 0) {
			fprintf(outf, "%s", names[i]->d_name);
		} else {
			fprintf(outf, "  %s", names[i]->d_name);
		}
		linelen += namelen + 2;
	}
	fprintf(outf, "\n");

	free(names);
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
	char *data = readarg(line);

	if (source == NULL || target == NULL) {
		fprintf(stderr, "Mount requires 2 arguments\n");
		return -1;
	}

	if (mount(source, target, fstype, 0, data) < 0) {
		perror("mount");
		return -1;
	}

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

int main() {
	outf = stdout;

	fprintf(outf, "XXSH %s\n", XXSH_VERSION);
	linenoiseHistorySetMaxLen(64);
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>

#ifndef XXSH_VERSION
#define XXSH_VERSION "version unknown"
#endif

#define commands \
	X(ls, "List files in a directory") \
	X(stat, "Get info about a file") \
	X(pwd, "Print the current working directory") \
	X(cat, "See the content of files") \
	X(cd, "Change directory") \
	X(help, "Show this help text") \
	X(exit, "Exit XXSH")

static int running = 1;

static int readarg(char **line, char **arg) {
	if (**line == '\0') {
		*arg = *line;
		return 0;
	}

	while (**line == ' ' || **line == '\t') *line += 1;
	*arg = *line;

	while (**line != ' ' && **line != '\t' && **line != '\n' && **line != '\0') *line += 1;
	if (**line != '\0') {
		**line = '\0';
		*line += 1;
	}

	return **arg != '\0';
}

static int ls_path(char *path) {
	struct dirent **names;
	int n = scandir(path, &names, NULL, alphasort);
	if (n < 0) {
		perror(path);
		return -1;
	}

	for (int i = 0; i < n; ++i) {
		printf("%s\n", names[i]->d_name);
	}

	free(names);
	return 0;
}

static int do_ls(char **line) {
	char *arg;
	readarg(line, &arg);
	if (*arg == '\0') {
		return ls_path(".");
	}

	char *arg2;
	readarg(line, &arg2);
	if (*arg2 == '\0') {
		return ls_path(arg);
	}

	printf("%s:\n", arg);
	int ret;
	if ((ret = ls_path(arg)) < 0) {
		return ret;
	}

	do {
		printf("%s\n", arg2);
		if ((ret = ls_path(arg2)) < 0) {
			return ret;
		}

		readarg(line, &arg2);
	} while (*arg2 != '\0');

	return 0;
}

static int do_stat(char **line) {
	char *arg;
	while (readarg(line, &arg)) {
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

		printf("%s: %o: %s:%s\n", arg, st.st_mode, uname, gname);
	}

	return 0;
}

static int do_pwd(char **line) {
	char path[4096];
	errno = 0;
	if (getcwd(path, sizeof(path)) == NULL) {
		perror("getcwd");
		return 0;
	}

	printf("%s\n", path);
	return -1;
}

static int do_cat(char **line) {
	char *arg;
	readarg(line, &arg);
	if (*arg == '\0') {
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

			fwrite(buf, 1, n, stdout);
		}

		fflush(stdout);
		if (fclose(f) < 0) {
			perror("fclose");
		}
	} while (readarg(line, &arg));

	return 0;
}

static int do_cd(char **line) {
	char *arg;
	if (readarg(line, &arg)) {
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

static int do_help(char **line) {
#define X(name, desc) printf("%s: %s\n", #name, desc);
	commands
#undef X

	return 0;
}

static int do_exit(char **line) {
	running = 0;
	return 0;
}

static int run(char *line) {
	char *command;
	if (!readarg(&line, &command)) {
		return 0;
	}

#define X(name, desc) if (strcmp(command, #name) == 0) return do_ ## name(&line);
	commands
#undef X

	fprintf(stderr, "Unknown command '%s'\n", command);
	return 1;
}

int main() {
	printf("XXSH %s\n", XXSH_VERSION);
	int ret = 0;
	char line[4096];
	while (running) {
		printf(">> ");
		if (fgets(line, sizeof(line), stdin) == NULL) {
			perror("stdin");
			ret = 1;
			break;
		}

		int ret;
		ret = run(line);
		if (ret < 0) {
			fprintf(stderr, ":( %i\n", ret);
		}
	}

	printf("Exit.\n");
	return ret;
}

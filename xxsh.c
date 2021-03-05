#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#define commands \
	X(ls, "List files in a directory") \
	X(pwd, "Print the current working directory") \
	X(cat, "See the content of files") \
	X(cd, "Change directory") \
	X(help, "Show this help text") \
	X(exit, "Exit XXSH")

static int running = 1;

static void readarg(char **line, char **arg) {
	if (**line == '\0') {
		*arg = *line;
		return;
	}

	while (**line == ' ' || **line == '\t') *line += 1;
	*arg = *line;

	while (**line != ' ' && **line != '\t' && **line != '\n' && **line != '\0') *line += 1;
	if (**line != '\0') {
		**line = '\0';
		*line += 1;
	}
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

		readarg(line, &arg);
	} while (*arg != '\0');

	return 0;
}

static int do_cd(char **line) {
	char *arg;
	readarg(line, &arg);
	if (*arg == '\0') {
		if (chdir("/") < 0) {
			perror("/");
			return -1;
		}
	} else {
		if (chdir(arg) < 0) {
			perror(arg);
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
	readarg(&line, &command);

#define X(name, desc) if (strcmp(command, #name) == 0) return do_ ## name(&line);
	commands
#undef X

	fprintf(stderr, "Unknown command '%s'\n", command);
	return 1;
}

int main() {
	char line[4096];
	while (running) {
		printf(">> ");
		if (fgets(line, sizeof(line), stdin) == NULL) {
			perror("stdin");
			return 1;
		}

		int ret;
		ret = run(line);
		if (ret < 0) {
			fprintf(stderr, ":( %i\n", ret);
		}
	}

	return 0;
}

/*
 * lttngtrace.c
 *
 * lttngtrace starts/stop system wide tracing around program execution.
 *
 * Copyright (c) 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * This file should be setuid root, and belong to a "tracing" group. Only users
 * part of the tracing group can trace and view the traces gathered.
 *
 * TODO: LTTng should support per-session tracepoint activation.
 * TODO: use mkstemp() and save last trace name in user's home directory.
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#if DEBUG
#define printf_dbg(fmt, args...)	printf(fmt, args)
#else
#define printf_dbg(fmt, ...)
#endif

static char *trace_path;
static char trace_path_pid[PATH_MAX];
static int autotrace;	/*
			 * Is the trace_path automatically chosen in /tmp ? Can
			 * we unlink if needed ?
			 */
static int sigfwd_pid;

static int recunlink(const char *dirname)
{
	DIR *dir;
	struct dirent *entry;
	char path[PATH_MAX];

	dir = opendir(dirname);
	if (dir == NULL) {
		if (errno == ENOENT)
			return 0;
		perror("Error opendir()");
		return -errno;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
			snprintf(path, (size_t) PATH_MAX, "%s/%s", dirname,
				 entry->d_name);
			if (entry->d_type == DT_DIR)
				recunlink(path);
			else
				unlink(path);
		}
	}
	closedir(dir);
	rmdir(dirname);

	return 0;
}

static int start_tracing(void)
{
	int ret;
	char command[PATH_MAX];

	if (autotrace) {
		ret = recunlink(trace_path);
		if (ret)
			return ret;
	}

	/*
	 * Create the directory in /tmp to deal with races (refuse if fail).
	 * Only allow user and group to read the trace data (to limit
	 * information disclosure).
	 */
	ret = mkdir(trace_path, S_IRWXU|S_IRWXG);
	if (ret) {
		perror("Trace directory creation error");
		return ret;
	}
	ret = system("ltt-armall > /dev/null");
	if (ret)
		return ret;

	ret = snprintf(command, PATH_MAX - 1,
		       "lttctl -C -w %s autotrace1 > /dev/null",
		       trace_path);
	ret = ret < 0 ? ret : 0;
	if (ret)
		return ret;
	ret = system(command);
	if (ret)
		return ret;
}

static int stop_tracing(uid_t uid, gid_t egid)
{
	int ret;

	ret = system("lttctl -D autotrace1 > /dev/null");
	if (ret)
		return ret;
	ret = system("ltt-disarmall > /dev/null");
	if (ret)
		return ret;
	/* Hand the trace back to the user after tracing is over */
	ret = chown(trace_path, uid, egid);
	if (ret) {
		perror("chown error");
		return ret;
	}
}

static int write_child_pid(pid_t pid, uid_t uid, gid_t gid)
{
	int fd;
	FILE *fp;
	int ret;

	/* Create the file as exclusive to deal with /tmp file creation races */
	fd = open(trace_path_pid, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC,
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	fp = fdopen(fd, "w");
	if (!fp) {
		perror("Error writing child pid");
		return -errno;
	}
	
	fprintf(fp, "%u", (unsigned int) pid);
	ret = fclose(fp);
	if (ret)
		perror("Error in fclose");
	/* Hand pid information file back to user */
	ret = chown(trace_path_pid, uid, gid);
	if (ret)
		perror("chown error");
	return ret;
}

static int parse_options(int argc, char *argv[], int *arg)
{
	int ret = 0;

	for (;;) {
		if (*arg >= argc
		    || argv[*arg][0] != '-'
		    || argv[*arg][0] == '\0'
		    || argv[*arg][1] == '\0'
		    || !strcmp(argv[*arg], "--"))
			break;
		switch (argv[*arg][1]) {
		case 'o':	if (*arg + 1 >= argc) {
					printf("Missing -o trace name\n");
					ret = -EINVAL;
					break;
				}
				trace_path = argv[*arg + 1];
				(*arg) += 2;
				break;
		default:	printf("Unknown option -%c\n", argv[*arg][1]);
				ret = -EINVAL;
				return ret;
		}
	}
	return ret;
}

static int init_trace_path(void)
{
	int ret;

	if (!trace_path) {
		trace_path = "/tmp/autotrace1";
		autotrace = 1;
	}
	ret = snprintf(trace_path_pid, PATH_MAX - 1, "%s/%s",
		       trace_path, "pid");
	ret = ret < 0 ? ret : 0;
	return ret;
}

static void sighandler(int signo, siginfo_t *siginfo, void *context)
{
	kill(sigfwd_pid, signo);
}

static int init_sighand(sigset_t *saved_mask)
{
	sigset_t sig_all_mask;
	int gret = 0, ret;

	/* Block signals */
	ret = sigfillset(&sig_all_mask);
	if (ret)
		perror("Error in sigfillset");
	gret = (gret == 0) ? ret : gret;
	ret = sigprocmask(SIG_SETMASK, &sig_all_mask, saved_mask);
	if (ret)
		perror("Error in sigprocmask");
	gret = (gret == 0) ? ret : gret;
	return gret;
}

static int forward_signals(pid_t pid, sigset_t *saved_mask)
{
	struct sigaction act;
	int gret = 0, ret;

	/* Forward SIGINT and SIGTERM */
	sigfwd_pid = pid;
	act.sa_sigaction = sighandler;
	act.sa_flags = SA_SIGINFO | SA_RESTART;
	sigemptyset(&act.sa_mask);
	ret = sigaction(SIGINT, &act, NULL);
	if (ret)
		perror("Error in sigaction");
	gret = (gret == 0) ? ret : gret;
	ret = sigaction(SIGTERM, &act, NULL);
	if (ret)
		perror("Error in sigaction");
	gret = (gret == 0) ? ret : gret;

	/* Reenable signals */
	ret = sigprocmask(SIG_SETMASK, saved_mask, NULL);
	if (ret)
		perror("Error in sigprocmask");
	gret = (gret == 0) ? ret : gret;
	return gret;
}

int main(int argc, char *argv[])
{
	uid_t euid, uid;
	gid_t egid, gid;
	pid_t pid;
	int gret = 0, ret = 0;
	int arg = 1;
	sigset_t saved_mask;

	if (argc < 2)
		return -ENOENT;

	euid = geteuid();
	uid = getuid();
	egid = getegid();
	gid = geteuid();

	if (euid != 0 && uid != 0) {
		printf("%s must be setuid root\n", argv[0]);
		return -EPERM;
	}

	printf_dbg("euid: %d\n", euid);
	printf_dbg("uid: %d\n", uid);
	printf_dbg("egid: %d\n", egid);
	printf_dbg("gid: %d\n", gid);

	if (arg < argc) {
		ret = parse_options(argc, argv, &arg);
		if (ret)
			return ret;
	}

	ret = init_trace_path();
	gret = (gret == 0) ? ret : gret;

	ret = init_sighand(&saved_mask);
	gret = (gret == 0) ? ret : gret;

	ret = start_tracing();
	if (ret)
		return ret;

	pid = fork();
	if (pid > 0) {		/* parent */
		int status;

		ret = forward_signals(pid, &saved_mask);
		gret = (gret == 0) ? ret : gret;
		pid = wait(&status);
		if (pid == -1)
			gret = (gret == 0) ? -errno : gret;

		ret = stop_tracing(uid, egid);
		gret = (gret == 0) ? ret : gret;
		ret = write_child_pid(pid, uid, egid);
		gret = (gret == 0) ? ret : gret;
	} else if (pid == 0) {	/* child */
		/* Drop root euid before executing child program */
		seteuid(uid);
		/* Reenable signals */
		ret = sigprocmask(SIG_SETMASK, &saved_mask, NULL);
		if (ret) {
			perror("Error in sigprocmask");
			return ret;
		}
		ret = execvp(argv[arg], &argv[arg]);
		if (ret)
			perror("Execution error");
		return ret;
	} else {		/* error */
		perror("Error in fork");
		return -errno;
	}
	return ret;
}

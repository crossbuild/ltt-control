/* lttd
 *
 * Linux Trace Toolkit Daemon
 *
 * This is a simple daemon that reads a few relay+debugfs channels and save
 * them in a trace.
 *
 * CPU hot-plugging is supported using inotify.
 *
 * Copyright 2009-2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _REENTRANT
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#include <liblttd/liblttd.h>

struct lttd_channel_data {
	int trace;
};

struct liblttd_instance *instance;
static char		path_trace[PATH_MAX];
static char		*end_path_trace;
static int		path_trace_len = 0;
static char		*trace_name = NULL;
static char		*channel_name = NULL;
static int		daemon_mode = 0;
static int		append_mode = 0;
static unsigned long	num_threads = 1;
static int		dump_flight_only = 0;
static int		dump_normal_only = 0;
static int		verbose_mode = 0;

static __thread int thread_pipe[2];

#define printf_verbose(fmt, args...) \
  do {                               \
    if (verbose_mode)                \
      printf(fmt, ##args);           \
  } while (0)

/* Args :
 *
 * -t directory		Directory name of the trace to write to. Will be created.
 * -c directory		Root directory of the debugfs trace channels.
 * -d          		Run in background (daemon).
 * -a			Trace append mode.
 * -s			Send SIGUSR1 to parent when ready for IO.
 */
void show_arguments(void)
{
	printf("Please use the following arguments :\n");
	printf("\n");
	printf("-t directory  Directory name of the trace to write to.\n"
				 "              It will be created.\n");
	printf("-c directory  Root directory of the debugfs trace channels.\n");
	printf("-d            Run in background (daemon).\n");
	printf("-a            Append to an possibly existing trace.\n");
	printf("-N            Number of threads to start.\n");
	printf("-f            Dump only flight recorder channels.\n");
	printf("-n            Dump only normal channels.\n");
	printf("-v            Verbose mode.\n");
	printf("\n");
}


/* parse_arguments
 *
 * Parses the command line arguments.
 *
 * Returns 1 if the arguments were correct, but doesn't ask for program
 * continuation. Returns -1 if the arguments are incorrect, or 0 if OK.
 */
int parse_arguments(int argc, char **argv)
{
	int ret = 0;
	int argn = 1;

	if(argc == 2) {
		if(strcmp(argv[1], "-h") == 0) {
			return 1;
		}
	}

	while(argn < argc) {

		switch(argv[argn][0]) {
			case '-':
				switch(argv[argn][1]) {
					case 't':
						if(argn+1 < argc) {
							trace_name = argv[argn+1];
							argn++;
						}
						break;
					case 'c':
						if(argn+1 < argc) {
							channel_name = argv[argn+1];
							argn++;
						}
						break;
					case 'd':
						daemon_mode = 1;
						break;
					case 'a':
						append_mode = 1;
						break;
					case 'N':
						if(argn+1 < argc) {
							num_threads = strtoul(argv[argn+1], NULL, 0);
							argn++;
						}
						break;
					case 'f':
						dump_flight_only = 1;
						break;
					case 'n':
						dump_normal_only = 1;
						break;
					case 'v':
						verbose_mode = 1;
						break;
					default:
						printf("Invalid argument '%s'.\n", argv[argn]);
						printf("\n");
						ret = -1;
				}
				break;
			default:
				printf("Invalid argument '%s'.\n", argv[argn]);
				printf("\n");
				ret = -1;
		}
		argn++;
	}

	if(trace_name == NULL) {
		printf("Please specify a trace name.\n");
		printf("\n");
		ret = -1;
	}

	if(channel_name == NULL) {
		printf("Please specify a channel name.\n");
		printf("\n");
		ret = -1;
	}

	return ret;
}

void show_info(void)
{
	printf("Linux Trace Toolkit Trace Daemon " VERSION "\n");
	printf("\n");
	printf("Reading from debugfs directory : %s\n", channel_name);
	printf("Writing to trace directory : %s\n", trace_name);
	printf("\n");
}


/* signal handling */

static void handler(int signo)
{
	printf("Signal %d received : exiting cleanly\n", signo);
	liblttd_stop_instance(instance);
}

int lttd_on_open_channel(struct liblttd_callbacks *data, struct fd_pair *pair, char *relative_channel_path)
{
	int open_ret = 0;
	int ret;
	struct stat stat_buf;
	struct lttd_channel_data *channel_data;

	pair->user_data = malloc(sizeof(struct lttd_channel_data));
	channel_data = pair->user_data;

	strncpy(end_path_trace, relative_channel_path, PATH_MAX - path_trace_len);
	printf_verbose("Creating trace file %s\n", path_trace);

	ret = stat(path_trace, &stat_buf);
	if(ret == 0) {
		if(append_mode) {
			printf_verbose("Appending to file %s as requested\n",
				path_trace);

			channel_data->trace = open(path_trace, O_WRONLY, S_IRWXU|S_IRWXG|S_IRWXO);
			if(channel_data->trace == -1) {
				perror(path_trace);
				open_ret = -1;
				goto end;
			}
			ret = lseek(channel_data->trace, 0, SEEK_END);
			if (ret < 0) {
				perror(path_trace);
				open_ret = -1;
				close(channel_data->trace);
				goto end;
			}
		} else {
			printf("File %s exists, cannot open. Try append mode.\n", path_trace);
			open_ret = -1;
			goto end;
		}
	} else {
		if(errno == ENOENT) {
			channel_data->trace = open(path_trace, O_WRONLY|O_CREAT|O_EXCL, S_IRWXU|S_IRWXG|S_IRWXO);
			if(channel_data->trace == -1) {
				perror(path_trace);
				open_ret = -1;
				goto end;
			}
		}
	}

end:
	return open_ret;

}

int lttd_on_close_channel(struct liblttd_callbacks *data, struct fd_pair *pair)
{
	int ret;
	ret = close(((struct lttd_channel_data *)(pair->user_data))->trace);
	free(pair->user_data);
	return ret;
}

int lttd_on_new_channels_folder(struct liblttd_callbacks *data, char *relative_folder_path)
{
	int ret;
	int open_ret = 0;

	strncpy(end_path_trace, relative_folder_path, PATH_MAX - path_trace_len);
	printf_verbose("Creating trace subdirectory %s\n", path_trace);

	ret = mkdir(path_trace, S_IRWXU|S_IRWXG|S_IRWXO);
	if(ret == -1) {
		if(errno != EEXIST) {
			perror(path_trace);
			open_ret = -1;
			goto end;
		}
	}

end:
	return open_ret;
}

int lttd_on_read_subbuffer(struct liblttd_callbacks *data, struct fd_pair *pair, unsigned int len)
{
	long ret;
	off_t offset = 0;

	while (len > 0) {
		printf_verbose("splice chan to pipe offset %lu\n",
			(unsigned long)offset);
		ret = splice(pair->channel, &offset, thread_pipe[1], NULL,
			len, SPLICE_F_MOVE | SPLICE_F_MORE);
		printf_verbose("splice chan to pipe ret %ld\n", ret);
		if (ret < 0) {
			perror("Error in relay splice");
			goto write_error;
		}
		ret = splice(thread_pipe[0], NULL,
			((struct lttd_channel_data *)(pair->user_data))->trace,
			NULL, ret, SPLICE_F_MOVE | SPLICE_F_MORE);
		printf_verbose("splice pipe to file %ld\n", ret);
		if (ret < 0) {
			perror("Error in file splice");
			goto write_error;
		}
		len -= ret;
	}

write_error:
	return ret;
}

int lttd_on_new_thread(struct liblttd_callbacks *data, unsigned long thread_num)
{
	int ret;
	ret = pipe(thread_pipe);
	if (ret < 0) {
		perror("Error creating pipe");
		return ret;
	}
	return 0;
}

int lttd_on_close_thread(struct liblttd_callbacks *data, unsigned long thread_num)
{
	close(thread_pipe[0]);	/* close read end */
	close(thread_pipe[1]);	/* close write end */
	return 0;
}

int main(int argc, char ** argv)
{
	int ret = 0;
	struct sigaction act;

	struct liblttd_callbacks callbacks = {
		lttd_on_open_channel,
		lttd_on_close_channel,
		lttd_on_new_channels_folder,
		lttd_on_read_subbuffer,
		NULL,
		lttd_on_new_thread,
		lttd_on_close_thread,
		NULL
	};

	ret = parse_arguments(argc, argv);

	if(ret != 0) show_arguments();
	if(ret < 0) return EINVAL;
	if(ret > 0) return 0;

	show_info();

	/* Connect the signal handlers */
	act.sa_handler = handler;
	act.sa_flags = 0;
	sigemptyset(&(act.sa_mask));
	sigaddset(&(act.sa_mask), SIGTERM);
	sigaddset(&(act.sa_mask), SIGQUIT);
	sigaddset(&(act.sa_mask), SIGINT);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	if(daemon_mode) {
		ret = daemon(0, 0);

		if(ret == -1) {
			perror("An error occured while daemonizing.");
			exit(-1);
		}
	}
	strncpy(path_trace, trace_name, PATH_MAX-1);
	path_trace_len = strlen(path_trace);
	end_path_trace = path_trace + path_trace_len;

	instance = liblttd_new_instance(&callbacks, channel_name, num_threads,
		dump_flight_only, dump_normal_only, verbose_mode);
	if(!instance) {
		perror("An error occured while creating the liblttd instance");
		return ret;
	}

	liblttd_start_instance(instance);

	return ret;
}


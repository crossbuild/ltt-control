/* liblttdutils
 *
 * Linux Trace Toolkit utility library
 *
 * This is a simple daemon implementation that reads a few relay+debugfs
 * channels and save them in a trace.
 *
 * CPU hot-plugging is supported using inotify.
 *
 * Copyright 2005 -
 * 	Mathieu Desnoyers <mathieu.desnoyers@polymtl.ca>
 * Copyright 2010 -
 *	Michael Sills-Lavoie <michael.sills-lavoie@polymtl.ca>
 *	Oumarou Dicko <oumarou.dicko@polymtl.ca>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _REENTRANT
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>

#include "liblttdutils.h"

struct liblttdutils_channel_data {
	int trace;
};

struct liblttdutils_data {
	char path_trace[PATH_MAX];
	char *end_path_trace;
	int path_trace_len;
	int append_mode;
	int verbose_mode;
};

static __thread int thread_pipe[2];

#define printf_verbose(fmt, args...) \
  do {                               \
    if (callbacks_data->verbose_mode)                \
      printf(fmt, ##args);           \
  } while (0)

int liblttdutils_local_on_open_channel(struct liblttd_callbacks *data, struct fd_pair *pair, char *relative_channel_path)
{
	int open_ret = 0;
	int ret;
	struct stat stat_buf;
	struct liblttdutils_channel_data *channel_data;

	pair->user_data = malloc(sizeof(struct liblttdutils_channel_data));
	channel_data = pair->user_data;

	struct liblttdutils_data* callbacks_data = data->user_data;

	strncpy(callbacks_data->end_path_trace, relative_channel_path, PATH_MAX - callbacks_data->path_trace_len);
	printf_verbose("Creating trace file %s\n", callbacks_data->path_trace);

	ret = stat(callbacks_data->path_trace, &stat_buf);
	if(ret == 0) {
		if(callbacks_data->append_mode) {
			printf_verbose("Appending to file %s as requested\n",
				callbacks_data->path_trace);

			channel_data->trace = open(callbacks_data->path_trace, O_WRONLY, S_IRWXU|S_IRWXG|S_IRWXO);
			if(channel_data->trace == -1) {
				perror(callbacks_data->path_trace);
				open_ret = -1;
				goto end;
			}
			ret = lseek(channel_data->trace, 0, SEEK_END);
			if (ret < 0) {
				perror(callbacks_data->path_trace);
				open_ret = -1;
				close(channel_data->trace);
				goto end;
			}
		} else {
			printf("File %s exists, cannot open. Try append mode.\n", callbacks_data->path_trace);
			open_ret = -1;
			goto end;
		}
	} else {
		if(errno == ENOENT) {
			channel_data->trace =
				open(callbacks_data->path_trace, O_WRONLY|O_CREAT|O_EXCL, S_IRWXU|S_IRWXG|S_IRWXO);
			if(channel_data->trace == -1) {
				perror(callbacks_data->path_trace);
				open_ret = -1;
				goto end;
			}
		}
	}

end:
	return open_ret;

}

int liblttdutils_local_on_close_channel(struct liblttd_callbacks *data, struct fd_pair *pair)
{
	int ret;
	ret = close(((struct liblttdutils_channel_data *)(pair->user_data))->trace);
	free(pair->user_data);
	return ret;
}

int liblttdutils_local_on_new_channels_folder(struct liblttd_callbacks *data, char *relative_folder_path)
{
	int ret;
	int open_ret = 0;
	struct liblttdutils_data* callbacks_data = data->user_data;

	strncpy(callbacks_data->end_path_trace, relative_folder_path, PATH_MAX - callbacks_data->path_trace_len);
	printf_verbose("Creating trace subdirectory %s\n", callbacks_data->path_trace);

	ret = mkdir(callbacks_data->path_trace, S_IRWXU|S_IRWXG|S_IRWXO);
	if(ret == -1) {
		if(errno != EEXIST) {
			perror(callbacks_data->path_trace);
			open_ret = -1;
			goto end;
		}
	}

end:
	return open_ret;
}

int liblttdutils_local_on_read_subbuffer(struct liblttd_callbacks *data, struct fd_pair *pair, unsigned int len)
{
	long ret;
	off_t offset = 0;

	struct liblttdutils_data* callbacks_data = data->user_data;

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
			((struct liblttdutils_channel_data *)(pair->user_data))->trace,
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

int liblttdutils_local_on_new_thread(struct liblttd_callbacks *data, unsigned long thread_num)
{
	int ret;
	ret = pipe(thread_pipe);
	if (ret < 0) {
		perror("Error creating pipe");
		return ret;
	}
	return 0;
}

int liblttdutils_local_on_close_thread(struct liblttd_callbacks *data, unsigned long thread_num)
{
	close(thread_pipe[0]);	/* close read end */
	close(thread_pipe[1]);	/* close write end */
	return 0;
}

int liblttdutils_local_on_trace_end(struct liblttd_instance *instance)
{
	struct liblttd_callbacks *callbacks = instance->callbacks;
	struct liblttdutils_data *data = callbacks->user_data;

	free(data);
	free(callbacks);
}

struct liblttd_callbacks* liblttdutils_local_new_callbacks(char* trace_name,
	int append_mode, int verbose_mode)
{
	struct liblttdutils_data *data;
	struct liblttd_callbacks *callbacks;

	if(!trace_name) return NULL;

	data = malloc(sizeof(struct liblttdutils_data));

	strncpy(data->path_trace, trace_name, PATH_MAX-1);
	data->path_trace_len = strlen(data->path_trace);
	data->end_path_trace = data->path_trace + data->path_trace_len;
	data->append_mode = append_mode;
	data->verbose_mode = verbose_mode;

	callbacks = malloc(sizeof(struct liblttd_callbacks));

	callbacks->on_open_channel = liblttdutils_local_on_open_channel;
	callbacks->on_close_channel = liblttdutils_local_on_close_channel;
	callbacks->on_new_channels_folder = liblttdutils_local_on_new_channels_folder;
	callbacks->on_read_subbuffer = liblttdutils_local_on_read_subbuffer;
	callbacks->on_trace_end = liblttdutils_local_on_trace_end;
	callbacks->on_new_thread = liblttdutils_local_on_new_thread;
	callbacks->on_close_thread = liblttdutils_local_on_close_thread;
	callbacks->user_data = data;

	return callbacks;
}


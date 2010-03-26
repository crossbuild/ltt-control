/* liblttd header file
 *
 * Copyright 2010-
 *		 Oumarou Dicko <oumarou.dicko@polymtl.ca>
 *		 Michael Sills-Lavoie <michael.sills-lavoie@polymtl.ca>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LIBLTTD_H
#define _LIBLTTD_H

#include <pthread.h>

/**
* This structure contains the data associated with the channel file descriptor.
* The lib user can use user_data to store the data associated to the specified
* channel. The lib user can read but MUST NOT change the other attributes.
*/
struct fd_pair {
	/**
	* This is the channel file descriptor.
	*/
	int channel;

	/**
	* This is the number of subbuffer for this channel.
	*/
	unsigned int n_sb;

	/**
	* This is the subbuffer size for this channel.
	*/
	unsigned int max_sb_size;

	/**
	* Not used anymore.
	*/
	void *mmap;

	/**
	* This is a mutex for internal library usage.
	*/
	pthread_mutex_t	mutex;

	/**
	* Library user data.
	*/
	void *user_data;
};

/**
* This structure contains the necessary callbacks for a tracing session. The
* user can set the unnecessary functions to NULL if he does not need them.
*/
struct liblttd_callbacks {
	/**
	* This callback is called after a channel file is open.
	*
	* @args data This argument is a pointeur to the callbacks struct that
	*            has been passed to the lib.
	* @args pair This structure contains the data associated with the
	*            channel file descriptor. The lib user can use user_data to
	*            store the data associated to the specified channel.
	* @args relative_channel_path This argument represents a relative path
	*            to the channel file. This path is relative to the root
	*            folder of the trace channels.
	*
	* @return Should return 0 if the callback succeeds else not 0.
	*/
	int(*on_open_channel)(struct liblttd_callbacks *data,
		struct fd_pair *pair, char *relative_channel_path);

	/**
	* This callback is called after a channel file is closed.
	*
	* @remarks After a channel file has been closed, it will never be read
	*            again.
	*
	* @args data This argument is a pointeur to the callbacks struct that
	*            has been passed to the lib.
	* @args pair This structure contains the data associated with the
	*            channel file descriptor. The lib user should clean
	*            user_data at this time.
	*
	* @return Should return 0 if the callback succeeds else not 0.
	*/
	int(*on_close_channel)(struct liblttd_callbacks *data,
		struct fd_pair *pair);


	/**
	* This callback is called when the library enter in a new subfolder
	* while it is scanning the trace channel tree. It can be used to create
	* the output file structure of the trace.
	*
	* @args data This argument is a pointeur to the callbacks struct that
	*            has been passed to the lib.
	* @args relative_folder_path This argument represents a relative path
	*            to the channel folder. This path is relative to the root
	*            folder of the trace channels.
	*
	* @return Should return 0 if the callback succeeds else not 0.
	*/
	int(*on_new_channels_folder)(struct liblttd_callbacks *data,
		char *relative_folder_path);

	/**
	* This callback is called after a subbuffer is a reserved.
	*
	* @attention It has to be thread safe, it'll be called by many threads.
	*
	* @args data This argument is a pointeur to the callbacks struct that
	*            has been passed to the lib.
	* @args pair This structure contains the data associated with the
	*            channel file descriptor. The lib user should clean
	*            user_data at this time.
	* @args len This argument represents the length the data that has to be
	*            read.
	*
	* @return Should return 0 if the callback succeeds else not 0.
	*/
	int(*on_read_subbuffer)(struct liblttd_callbacks *data,
		struct fd_pair *pair, unsigned int len);

	/**
	* This callback is called at the very end of the tracing session. At
	* this time, all the channels have been closed and the threads have been
	* destroyed.
	*
	* @remarks After this callback is called, no other callback will be
	*            called again.
	*
	* @attention It has to be thread safe, it'll be called by many threads.
	*
	* @args data This argument is a pointeur to the callbacks struct that
	*            has been passed to the lib.
	*
	* @return Should return 0 if the callback succeeds else not 0.
	*/
	int(*on_trace_end)(struct liblttd_callbacks *data);

	/**
	* This callback is called after a new thread has been created.
	*
	* @attention It has to be thread safe, it'll be called by many threads.
	*
	* @args data This argument is a pointeur to the callbacks struct that
	*            has been passed to the lib.
	* @args thread_num This argument represents the id of the thread.
	*
	* @return Should return 0 if the callback succeeds else not 0.
	*/
	int(*on_new_thread)(struct liblttd_callbacks *data,
		unsigned long thread_num);

	/**
	* This callback is called just before a thread is destroyed.
	*
	* @attention It has to be thread safe, it'll be called by many threads.
	*
	* @args data This argument is a pointeur to the callbacks struct that
	*            has been passed to the lib.
	* @args thread_num This argument represents the number of the thread.
	*
	* @return Should return 0 if the callback succeeds else not 0.
	*/
	int(*on_close_thread)(struct liblttd_callbacks *data,
		unsigned long thread_num);

	/**
	* This is where the user can put the library's data.
	*/
	void *user_data;
};

/**
* This function is called to start a new tracing session.
*
* @attention It has to be thread safe, it'll be called by many threads.
*
* @args channel_path This argument is a path to the root folder of the trace's
*            channels.
* @args n_threads This argument represents the number of threads that will be
*            used by the library.
* @args flight_only If this argument to set to 1, only the channel that are in
*            flight recorder mode will be recorded.
* @args normal_only If this argument to set to 1, only the channel that are in
*            normal mode will be recorded.
* @args verbose If this argument to set to 1, more informations will be printed.
* @args user_data This argument is a pointeur to the callbacks struct that
*            contains the user's functions.
*
* @return Return 0 if the function succeeds else not 0.
*/
int liblttd_start(char *channel_path, unsigned long n_threads,
	int flight_only, int normal_only, int verbose,
	struct liblttd_callbacks *user_data);

/**
* This function is called to stop a tracing session.
*
* @return Return 0 if the function succeeds.
*/
int liblttd_stop();

#endif /*_LIBLTTD_H */


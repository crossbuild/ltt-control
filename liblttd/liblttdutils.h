/* liblttdutils header file
 *
 * Copyright 2010-
 *		 Oumarou Dicko <oumarou.dicko@polymtl.ca>
 *		 Michael Sills-Lavoie <michael.sills-lavoie@polymtl.ca>
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

#ifndef _LIBLTTDUTILS_H
#define _LIBLTTDUTILS_H

#include "liblttd.h"

/**
 * liblttdutils_new_callbacks - Is a utility function called to create a new
 * callbacks struct used by liblttd to write trace data to the disk.
 *
 * @trace_name: Directory name of the trace to write to. It will be created.
 * @append_mode: Append to a possibly existing trace.
 * @verbose_mode: Verbose mode.
 *
 * Returns the callbacks if the function succeeds else NULL.
 */
struct liblttd_callbacks* liblttdutils_local_new_callbacks(char* trace_name,
	int append_mode, int verbose_mode);

#endif /*_LIBLTTDUTILS_H */


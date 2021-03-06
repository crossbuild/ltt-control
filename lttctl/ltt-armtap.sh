# Copyright 2009 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# This script will enable the system-wide tap on the given list of events passed
# as parameter, and stop the tap at each other "normal rate" events.

#excluding core markers (already connected)
#excluding locking markers (high traffic)

echo Connecting function markers

# interesting period starts with the list of events passed as parameter.
START_FTRACE=$*

# interesting period may stop with one specific event, but also try to keep the
# other START_FTRACE events triggers to the lowest possible overhead by stopping
# function trace at every other events.
# Do _not_ disable function tracing in ftrace_entry event unless you really only
# want the first function entry...
STOP_FTRACE=`cat /proc/ltt|grep -v %k|awk '{print $2}'|sort -u|grep -v ^core_|grep -v ^locking_|grep -v ^lockdep|grep -v ftrace_entry|grep -v ^tap_`

for a in $START_FTRACE; do
	STOP_FTRACE=`echo $STOP_FTRACE|sed 's/$a//'`
done


for a in $STOP_FTRACE; do
	echo Connecting stop $a
	echo "connect $a ftrace_system_stop" > /proc/ltt
done

for a in $START_FTRACE; do
	echo Connecting start $a
	echo "connect $a ftrace_system_start" > /proc/ltt
done



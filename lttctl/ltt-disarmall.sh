# Copyright (C) 2009 Benjamin Poirier
# Copyright (C) 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

DEBUGFSROOT=$(awk '{if ($3 == "debugfs") print $2}' /proc/mounts | head -n 1)
MARKERSROOT=${DEBUGFSROOT}/ltt/markers
DEFAULTMODULES="ltt-trace-control ltt-marker-control ltt-kprobes ltt-userspace-event ltt-statedump ipc-trace kernel-trace mm-trace net-trace fs-trace jbd2-trace syscall-trace trap-trace block-trace rcu-trace ltt-relay ltt-tracer"
EXTRAMODULES="lockdep-trace net-extended-trace"

usage () {
	echo "Usage: $0 [OPTION]..." 1>&2
	echo "Disconnect lttng markers" 1>&2
	echo "" 1>&2
	echo "Options:" 1>&2
	printf "\t-q           Quiet mode, suppress output\n" 1>&2
	printf "\t-h           Print this help\n" 1>&2
	echo "" 1>&2
}

if [ "$(id -u)" != "0" ]; then
	echo "Error: This script needs to be run as root." 1>&2
	exit 1;
fi

if [ ! "${DEBUGFSROOT}" ]; then
	echo "Error: debugfs not mounted" 1>&2
	exit 1;
fi

if [ ! -d "${MARKERSROOT}" ]; then
	echo "Error: LTT trace controller not found (did you compile and load LTTng?)" 1>&2
	exit 1;
fi

while getopts "qh" options; do
	case ${options} in
		q) QUIET="0";;
		h) usage;
			exit 0;;
		\?) usage;
			exit 1;;
	esac
done
shift $((${OPTIND} - 1))

(eval "find '${MARKERSROOT}' -name metadata -prune -o -name enable -print") | while read -r marker; do
	grep "^1$" "${marker}" -q
	if [ $? -ne 0 ]; then
		continue
	fi
	if [ ! ${QUIET} ]; then
		echo "Disconnecting ${marker%/enable}"
	fi
	echo 0 > ${marker}
done

#Unload the kernel modules
for i in ${EXTRAMODULES}; do
	rmmod $i 2> /dev/null
done
for i in ${DEFAULTMODULES}; do
	rmmod $i
done


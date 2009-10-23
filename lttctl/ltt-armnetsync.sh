DEBUGFSROOT=$(grep ^debugfs /proc/mounts | head -1 | awk '{print $2}')
MARKERSROOT=${DEBUGFSROOT}/ltt/markers

echo Connecting network synchronization markers

for m in ${MARKERSROOT}/net/*_extended; do
	echo Connecting ${m}
	echo 1 > ${m}/enable
done
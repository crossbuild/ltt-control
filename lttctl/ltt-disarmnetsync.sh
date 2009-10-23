DEBUGFSROOT=$(grep ^debugfs /proc/mounts | head -1 | awk '{print $2}')
MARKERSROOT=${DEBUGFSROOT}/ltt/markers

echo Disconnecting network synchronization markers

for m in ${MARKERSROOT}/net/*_extended; do
	echo Disconnecting ${m}
	echo 0 > ${m}/enable
done
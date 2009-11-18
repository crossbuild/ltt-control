DEBUGFSROOT=$(grep ^debugfs /proc/mounts | head -1 | awk '{print $2}')
MARKERSROOT=${DEBUGFSROOT}/ltt/markers

echo Disconnecting all markers

for c in ${MARKERSROOT}/*; do
	case ${c} in
	${MARKERSROOT}/metadata)
		;;
	${MARKERSROOT}/locking)
		;;
	${MARKERSROOT}/lockdep)
		;;
	*)
		for m in ${c}/*; do
			echo Disconnecting ${m}
			echo 0 > ${m}/enable
		done
		;;
	esac
done

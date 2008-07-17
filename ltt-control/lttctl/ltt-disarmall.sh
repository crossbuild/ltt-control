#excluding locking
#excluding core markers, not connected to default.
echo Disconnecting all markers
MARKERS=`cat /proc/ltt|grep -v %k|awk '{print $2}'|sort -u|grep -v ^core_|grep -v ^locking_`
for a in $MARKERS; do echo Disconnecting $a; echo "disconnect $a" > /proc/ltt; done
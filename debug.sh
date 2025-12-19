#! /bin/bash

while test 1
do
	echo "$(date) - $(dd if=/dev/random bs=1 count=$[$RANDOM%15+5] 2>/dev/null | base64)"
	sleep ${SLEEP:-5}
done

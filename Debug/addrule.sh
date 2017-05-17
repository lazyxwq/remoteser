#!/bin/bash

port=$1

#`iptables-save > iptables.save`
#echo $?
#`iptables-restore iptables.save`
#echo $?
existsport=`iptables -nL --line-number | grep "tcp dpt:$port" | awk '{print $1}'`
if [ "$existsport" != "" ]; then
	echo "1"
	exit 1
fi
`iptables -I INPUT 1 -p tcp -m state --state NEW -m tcp --dport $port -j ACCEPT `
sleep 1
echo $?

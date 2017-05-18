#!/bin/bash

port=$1
num=`iptables -nL --line-number | grep "tcp dpt:$port" | awk '{print $1}'`
`iptables -D INPUT $num`
sleep 1

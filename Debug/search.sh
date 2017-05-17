#!/bin/bash

port=$1

echo "iptables -nL --line-number | grep "tcp dpt:$port" | awk '{print \$1}'"
iptables -nL --line-number | grep "tcp dpt:$port" | awk '{print $1}'

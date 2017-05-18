#!/bin/bash

iptables -nL --line-number > /tmp/showlist 2>&1
cat /tmp/showlist

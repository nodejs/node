#!/bin/bash
cd "$(dirname "$(dirname $0)")"
sudo sysctl -w net.inet.ip.portrange.first=12000
sudo sysctl -w net.inet.tcp.msl=1000
sudo sysctl -w kern.maxfiles=1000000 kern.maxfilesperproc=1000000
ulimit -n 100000
./node benchmark/http_simple.js || exit 1 &
sleep 1
ab -n 30000 -c 100 http://127.0.0.1:8000/bytes/123 | grep Req
killall node

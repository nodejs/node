#!/bin/bash
cd "$(dirname "$(dirname $0)")"
sudo sysctl -w net.inet.ip.portrange.first=12000
sudo sysctl -w net.inet.tcp.msl=1000
sudo sysctl -w kern.maxfiles=1000000 kern.maxfilesperproc=1000000
ulimit -n 100000

k=${KEEPALIVE}
if [ "$k" = "no" ]; then
  k=""
else
  k="-k"
fi
node=${NODE:-./node}

$node benchmark/http_simple.js &
npid=$!
sleep 1

if [ "$k" = "-k" ]; then
  echo "using keepalive"
fi

for i in a a a a a a a a a a a a a a a a a a a a; do
  ab $k -t 10 -c 100 http://127.0.0.1:8000/${TYPE:-bytes}/${LENGTH:-1024} \
    2>&1 | grep Req | egrep -o '[0-9\.]+'
done

kill $npid

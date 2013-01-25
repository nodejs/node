#!/bin/bash
cd "$(dirname "$(dirname $0)")"

if type sysctl &>/dev/null; then
  # darwin and linux
  sudo sysctl -w net.ipv4.ip_local_port_range="12000 65535"
  sudo sysctl -w net.inet.ip.portrange.first=12000
  sudo sysctl -w net.inet.tcp.msl=1000
  sudo sysctl -w kern.maxfiles=1000000 kern.maxfilesperproc=1000000
elif type /usr/sbin/ndd &>/dev/null; then
  # sunos
  /usr/sbin/ndd -set /dev/tcp tcp_smallest_anon_port 12000
  /usr/sbin/ndd -set /dev/tcp tcp_largest_anon_port 65535
  /usr/sbin/ndd -set /dev/tcp tcp_max_buf 2097152
  /usr/sbin/ndd -set /dev/tcp tcp_xmit_hiwat 1048576
  /usr/sbin/ndd -set /dev/tcp tcp_recv_hiwat 1048576
fi

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

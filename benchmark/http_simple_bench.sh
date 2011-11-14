#!/bin/bash

SERVER=127.0.0.1
PORT=${PORT:=8000}

# You may want to configure your TCP settings to make many ports available
# to node and ab. On macintosh use: 
#   sudo sysctl -w net.inet.ip.portrange.first=32768
#   sudo sysctl -w net.inet.tcp.msl=1000

if [ ! -d benchmark/ ]; then
  echo "Run this script from the node root directory"
  exit 1
fi

if [ $SERVER == "127.0.0.1" ]; then
  ./node benchmark/http_simple.js &
  node_pid=$!
  sleep 1
fi

date=`date "+%Y%m%d%H%M%S"`

ab_hello_world() {
  local type="$1"
  local ressize="$2"
  if [ $type == "string" ]; then 
    local uri="bytes/$ressize"
  else
    local uri="buffer/$ressize"
  fi


  name="ab-hello-world-$type-$ressize"

  dir=".benchmark_reports/$name/$rev/"
  if [ ! -d $dir ]; then
    mkdir -p $dir
  fi

  summary_fn="$dir/$date.summary"
  data_fn="$dir/$date.data"

  echo "Bench $name starts in 3 seconds..."
  # let shit calm down
  sleep 3

  # hammer that as hard as it can for 10 seconds.
  ab -g $data_fn -c 100 -t 10 http://$SERVER:$PORT/$uri > $summary_fn

  # add our data about the server
  echo >> $summary_fn
  echo >> $summary_fn
  echo "webserver-rev: $rev" >> $summary_fn
  echo "webserver-uname: $uname" >> $summary_fn

  grep Req $summary_fn 

  echo "Summary: $summary_fn"
  echo
}

# 1k
ab_hello_world 'string' '1024'
ab_hello_world 'buffer' '1024'

# 100k 
ab_hello_world 'string' '102400'
ab_hello_world 'buffer' '102400'


if [ ! -z $node_pid ]; then
  kill -9 $node_pid
fi

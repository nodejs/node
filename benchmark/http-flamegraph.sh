#!/bin/bash
cd "$(dirname "$(dirname $0)")"

node=${NODE:-./node}

if type sysctl &>/dev/null; then
  sudo sysctl -w net.inet.ip.portrange.first=12000
  sudo sysctl -w net.inet.tcp.msl=1000
  sudo sysctl -w kern.maxfiles=1000000 kern.maxfilesperproc=1000000
fi
ulimit -n 100000

$node benchmark/http_simple.js &
nodepid=$!
echo "node pid = $nodepid"
sleep 1

# has to stay alive until dtrace exits
dtrace -n 'profile-97/pid == '$nodepid' && arg1/{ @[jstack(150, 8000)] = count(); } tick-60s { exit(0); }' \
  | grep -v _ZN2v88internalL21Builtin_HandleApiCallENS0_12_GLOBAL__N_116BuiltinA \
  > stacks.src &

dtracepid=$!

echo "dtrace pid = $dtracepid"

sleep 1

test () {
  c=$1
  t=$2
  l=$3
  k=$4
  ab $k -t 10 -c $c http://127.0.0.1:8000/$t/$l \
    2>&1 | grep Req
}

#test 100 bytes 1024
#test 10  bytes 100 -k
#test 100 bytes 1024 -k
#test 100 bytes 1024 -k
#test 100 bytes 1024 -k

echo 'Keep going until dtrace stops listening...'
while pargs $dtracepid &>/dev/null; do
  test 100 bytes 1 -k
done

kill $nodepid

echo 'Pluck off all the stacks that only happen once.'

node -e '
var fs = require("fs");
var data = fs.readFileSync("stacks.src", "utf8").split(/\n/);
var out = fs.createWriteStream("stacks.out");
function write(l) {
  out.write(l + "\n");
}
write(data[0]);
write(data[1]);
write(data[2]);

var stack = [];
var i = 4;
var saw2 = false
for (; i < data.length && !saw2; i++) {
  if (data[i] === "                2") {
    stack.forEach(function(line) {
      write(line);
    });
    write(data[i]);
    saw2 = true;
  } else if (data[i] === "                1") {
    stack = [];
  } else {
    stack.push(data[i]);
  }
}

for (; i < data.length; i++)
  write(data[i]);
'

echo 'Turn the stacks into a svg'
stackvis dtrace flamegraph-svg < stacks.out > stacks.svg

echo ''
echo 'done. Results in stacks.svg'

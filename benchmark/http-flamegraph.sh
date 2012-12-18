#!/bin/bash
cd "$(dirname "$(dirname $0)")"
if type sysctl &>/dev/null; then
  sudo sysctl -w net.inet.ip.portrange.first=12000
  sudo sysctl -w net.inet.tcp.msl=1000
  sudo sysctl -w kern.maxfiles=1000000 kern.maxfilesperproc=1000000
fi
ulimit -n 100000

./node benchmark/http_simple.js &
nodepid=$!
echo "node pid = $nodepid"
sleep 1

# has to stay alive until dtrace exits
dtrace -n 'profile-97/pid == '$nodepid' && arg1/{ @[jstack(150, 8000)] = count(); } tick-60s { exit(0); }' > stacks.src &
dtracepid=$!
echo "dtrace pid = $dtracepid"

test () {
  n=$1
  c=$2
  k=$3
  ab $k -n $n -c $c http://127.0.0.1:8000/${TYPE:-bytes}/${LENGTH:-1024} \
    2>&1 | grep Req
}

test 10000 100
test 10000 10 -k
test 10000 100 -k
test 30000 100 -k
test 60000 100 -k

echo 'Keep going until dtrace stops listening...'
while pargs $dtracepid &>/dev/null; do
  ab -k -n 60000 -c 100 http://127.0.0.1:8000/${TYPE:-bytes}/${LENGTH:-1024} 2>&1 | grep Req
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

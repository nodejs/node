#!/bin/bash
cd "$(dirname "$(dirname $0)")"

node=${NODE:-./node}

name=${NAME:-stacks}

if type sysctl &>/dev/null; then
  # darwin and linux
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
$node benchmark/http_simple.js &
nodepid=$!
echo "node pid = $nodepid"
sleep 1

# has to stay alive until dtrace exits
dtrace -n 'profile-97/pid == '$nodepid' && arg1/{ @[jstack(150, 8000)] = count(); } tick-60s { exit(0); }' \
  | grep -v _ZN2v88internalL21Builtin_HandleApiCallENS0_12_GLOBAL__N_116BuiltinA \
  > "$name".src &

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
  test 100 bytes ${LENGTH:-1} -k
done

kill $nodepid

echo 'Turn the stacks into a svg'
stackvis dtrace flamegraph-svg < "$name".src > "$name".raw.svg

echo 'Prune tiny stacks out of the graph'
node -e '
var infile = process.argv[1];
var outfile = process.argv[2];
var output = "";
var fs = require("fs");
var input = fs.readFileSync(infile, "utf8");

input = input.split("id=\"details\" > </text>");
var head = input.shift() + "id=\"details\" > </text>";
input = input.join("id=\"details\" > </text>");

var tail = "</svg>";
input = input.split("</svg>")[0];

var minyKept = Infinity;
var minyOverall = Infinity;
var rects = input.trim().split(/\n/).filter(function(rect) {
  var my = rect.match(/y="([0-9\.]+)"/);

  if (!my)
    return false;
  var y = +my[1];
  if (!y)
    return false;
  minyOverall = Math.min(minyOverall, y);

  // pluck off everything that will be less than one pixel.
  var mw = rect.match(/width="([0-9\.]+)"/)
  if (mw) {
    var width = +mw[1];
    if (!(width >= 1))
      return false;
  }
  minyKept = Math.min(minyKept, y);
  return true;
});

// move everything up to the top of the page.
var ydiff = minyKept - minyOverall;
rects = rects.map(function(rect) {
  var my = rect.match(/y="([0-9\.]+)"/);
  var y = +my[1];
  var newy = y - ydiff;
  rect = rect.replace(/y="([0-9\.]+)"/, "y=\"" + newy + "\"");
  return rect;
});

fs.writeFileSync(outfile, head + "\n" + rects.join("\n") + "\n" + tail);
' "$name".raw.svg "$name".svg

echo ''
echo 'done. Results in '"$name"'.svg'

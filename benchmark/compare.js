var usage = 'node benchmark/compare.js <node-binary1> <node-binary2> [--html] [--red|-r] [--green|-g]';

var show = 'both';
var nodes = [];
var html = false;

for (var i = 2; i < process.argv.length; i++) {
  var arg = process.argv[i];
  switch (arg) {
    case '--red': case '-r':
      show = show === 'green' ? 'both' : 'red';
      break;
    case '--green': case '-g':
      show = show === 'red' ? 'both' : 'green';
      break;
    case '--html':
      html = true;
      break;
    case '-h': case '-?': case '--help':
      console.log(usage);
      process.exit(0);
    default:
      nodes.push(arg);
      break;
  }
}

if (!html) {
  var start = '';
  var green = '\033[1;32m';
  var red = '\033[1;31m';
  var reset = '\033[m';
  var end = '';
} else {
  var start = '<pre style="background-color:#333;color:#eee">';
  var green = '<span style="background-color:#0f0;color:#000">';
  var red = '<span style="background-color:#f00">';
  var reset = '</span>';
  var end = '</pre>';
}

var runBench = process.env.NODE_BENCH || 'bench';

if (nodes.length !== 2)
	return console.error('usage:\n  %s', usage);

var spawn = require('child_process').spawn;
var results = {};
var n = 2;

run();

function run() {
  if (n === 0)
		return compare();

  n--;

	var node = nodes[n];
  console.error('running %s', node);
	var env = {};
  for (var i in process.env)
    env[i] = process.env[i];
  env.NODE = node;
	var child = spawn('make', [ runBench ], { env: env });

	var out = '';
	child.stdout.setEncoding('utf8');
	child.stdout.on('data', function(c) {
		out += c;
	});

  child.stderr.pipe(process.stderr);

	child.on('close', function(code) {
		if (code) {
			console.error('%s exited with code=%d', node, code);
			process.exit(code);
		} else {
			out.trim().split(/\r?\n/).forEach(function(line) {
        line = line.trim();
        if (!line)
          return;

				var s = line.split(':');
				var num = +s.pop();
				if (!num && num !== 0)
          return

				line = s.join(':');
				var res = results[line] = results[line] || {};
				res[node] = num;
			});

			run();
		}
	});
}

function compare() {
	// each result is an object with {"foo.js arg=bar":12345,...}
	// compare each thing, and show which node did the best.
  // node[0] is shown in green, node[1] shown in red.
  var maxLen = -Infinity;
  var util = require('util');
  console.log(start);

	Object.keys(results).map(function(bench) {
    var res = results[bench];
    var n0 = res[nodes[0]];
    var n1 = res[nodes[1]];

    var pct = ((n0 - n1) / n1 * 100).toFixed(2);

    var g = n0 > n1 ? green : '';
    var r = n0 > n1 ? '' : red;
    var c = r || g;

    if (show === 'green' && !g || show === 'red' && !r)
      return;

    var r0 = util.format('%s%s: %d%s', g, nodes[0], n0, reset);
    var r1 = util.format('%s%s: %d%s', r, nodes[1], n1, reset);
    var pct = c + pct + '%' + reset;
    var l = util.format('%s: %s %s', bench, r0, r1);
    maxLen = Math.max(l.length + pct.length, maxLen);
    return [l, pct];
  }).filter(function(l) {
    return l;
  }).forEach(function(line) {
    var l = line[0];
    var pct = line[1];
    var dotLen = maxLen - l.length - pct.length + 2;
    var dots = ' ' + new Array(Math.max(0, dotLen)).join('.') + ' ';
    console.log(l + dots + pct);
  });
  console.log(end);
}

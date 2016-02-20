'use strict';
var usage = 'node benchmark/compare.js ' +
            '<node-binary1> <node-binary2> ' +
            '[--html] [--red|-r] [--green|-g] ' +
            '[-- <type> [testFilter]]';

var show = 'both';
var nodes = [];
var html = false;
var benchmarks;

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
      break;
    case '--':
      benchmarks = [];
      break;
    default:
      if (Array.isArray(benchmarks))
        benchmarks.push(arg);
      else
        nodes.push(arg);
      break;
  }
}

var start, green, red, reset, end;
if (!html) {
  start = '';
  green = '\u001b[1;32m';
  red = '\u001b[1;31m';
  reset = '\u001b[m';
  end = '';
} else {
  start = '<pre style="background-color:#333;color:#eee">';
  green = '<span style="background-color:#0f0;color:#000">';
  red = '<span style="background-color:#f00;color:#fff">';
  reset = '</span>';
  end = '</pre>';
}

var runBench = process.env.NODE_BENCH || 'bench';

if (nodes.length !== 2)
  return console.error('usage:\n  %s', usage);

var spawn = require('child_process').spawn;
var results = {};
var toggle = 1;
var r = (+process.env.NODE_BENCH_RUNS || 1) * 2;

run();
function run() {
  if (--r < 0)
    return compare();
  toggle = ++toggle % 2;

  var node = nodes[toggle];
  console.error('running %s', node);
  var env = {};
  for (var i in process.env)
    env[i] = process.env[i];
  env.NODE = node;

  var out = '';
  var child;
  if (Array.isArray(benchmarks) && benchmarks.length) {
    child = spawn(
      node,
      ['benchmark/common.js'].concat(benchmarks),
      { env: env }
    );
  } else {
    child = spawn('make', [runBench], { env: env });
  }
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
          return;

        line = s.join(':');
        var res = results[line] = results[line] || {};
        res[node] = res[node] || [];
        res[node].push(num);
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
    var n0 = avg(res[nodes[0]]);
    var n1 = avg(res[nodes[1]]);

    var pct = ((n0 - n1) / n1 * 100).toFixed(2);

    var g = n0 > n1 ? green : '';
    var r = n0 > n1 ? '' : red;
    var c = r || g;

    if (show === 'green' && !g || show === 'red' && !r)
      return;

    var r0 = util.format(
      '%s%s: %d%s',
      g,
      nodes[0],
      n0.toPrecision(5), g ? reset : ''
    );
    var r1 = util.format(
      '%s%s: %d%s',
      r,
      nodes[1],
      n1.toPrecision(5), r ? reset : ''
    );
    pct = c + pct + '%' + reset;

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

function avg(list) {
  if (list.length >= 3) {
    list = list.sort();
    var q = Math.floor(list.length / 4) || 1;
    list = list.slice(q, -q);
  }
  return list.reduce(function(a, b) {
    return a + b;
  }, 0) / list.length;
}

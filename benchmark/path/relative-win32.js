'use strict';
var common = require('../common.js');
var path = require('path');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  paths: [
    ['C:\\orandea\\test\\aaa', 'C:\\orandea\\impl\\bbb'].join('|'),
    ['C:\\', 'D:\\'].join('|'),
    ['C:\\foo\\bar\\baz', 'C:\\foo\\bar\\baz'].join('|'),
    ['C:\\foo\\BAR\\BAZ', 'C:\\foo\\bar\\baz'].join('|'),
    ['C:\\foo\\bar\\baz\\quux', 'C:\\'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.win32;
  var from = '' + conf.paths;
  var to = '';
  var delimIdx = from.indexOf('|');
  if (delimIdx > -1) {
    to = from.slice(delimIdx + 1);
    from = from.slice(0, delimIdx);
  }

  // Force optimization before starting the benchmark
  p.relative(from, to);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.relative)');
  p.relative(from, to);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.relative(from, to);
  }
  bench.end(n);
}

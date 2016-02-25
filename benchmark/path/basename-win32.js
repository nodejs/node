'use strict';
var common = require('../common.js');
var path = require('path');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  pathext: [
    '',
    'C:\\',
    'C:\\foo',
    'D:\\foo\\.bar.baz',
    ['E:\\foo\\.bar.baz', '.baz'].join('|'),
    'foo',
    'foo\\bar.',
    ['foo\\bar.', '.'].join('|'),
    '\\foo\\bar\\baz\\asdf\\quux.html',
    ['\\foo\\bar\\baz\\asdf\\quux.html', '.html'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.win32;
  var input = '' + conf.pathext;
  var ext;
  var extIdx = input.indexOf('|');
  if (extIdx !== -1) {
    ext = input.slice(extIdx + 1);
    input = input.slice(0, extIdx);
  }

  // Force optimization before starting the benchmark
  p.basename(input, ext);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.basename)');
  p.basename(input, ext);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.basename(input, ext);
  }
  bench.end(n);
}

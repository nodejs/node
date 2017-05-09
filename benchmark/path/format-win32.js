'use strict';
var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  props: [
    ['C:\\', 'C:\\path\\dir', 'index.html', '.html', 'index'].join('|')
  ],
  n: [1e7]
});

function main(conf) {
  var n = +conf.n;
  var p = path.win32;
  var props = String(conf.props).split('|');
  var obj = {
    root: props[0] || '',
    dir: props[1] || '',
    base: props[2] || '',
    ext: props[3] || '',
    name: props[4] || '',
  };

  bench.start();
  for (var i = 0; i < n; i++) {
    p.format(obj);
  }
  bench.end(n);
}

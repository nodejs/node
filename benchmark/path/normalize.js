var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  type: ['win32', 'posix'],
  n: [1e6],
});

function main(conf) {
  var n = +conf.n;
  var p = path[conf.type];

  bench.start();
  for (var i = 0; i < n; i++) {
    p.normalize('/foo/bar//baz/asdf/quux/..');
  }
  bench.end(n);
}

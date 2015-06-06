var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  type: ['win32', 'posix'],
  n: [1e6],
});

function main(conf) {
  var n = +conf.n;
  var p = path[conf.type];
  var tests = conf.type === 'win32'
    ? ['//server', 'C:\\baz\\..', 'bar\\baz', '.']
    : ['/foo/bar', '/baz/..', 'bar/baz', '.'];

  bench.start();
  for (var i = 0; i < n; i++) {
    runTests(p, tests);
  }
  bench.end(n);
}

function runTests(p, tests) {
  for (var i = 0; i < tests.length; i++) {
    p.isAbsolute(tests[i]);
  }
}

var util = require('util');

var common = require('../common.js');

var bench = common.createBenchmark(main, {n: [5e6]});

function main(conf) {
  var n = conf.n | 0;

  bench.start();
  for (var i = 0; i < n; i += 1) {
    var r = util.inspect({a: 'a', b: 'b', c: 'c', d: 'd'});
  }
  bench.end(n);
}

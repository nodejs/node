var common = require('../common.js');
var EventEmitter = require('events').EventEmitter;

var bench = common.createBenchmark(main, {n: [25e6]});

function main(conf) {
  var n = conf.n | 0;

  var ee = new EventEmitter();

  function noop() {}

  bench.start();
  for (var i = 0; i < n; i += 1) {
    ee.once('dummy', noop);
    ee.emit('dummy');
  }
  bench.end(n);
}

var common = require('../common.js');
var EventEmitter = require('events').EventEmitter;

var bench = common.createBenchmark(main, {n: [25e6]});

function main(conf) {
  var n = conf.n | 0;

  var ee = new EventEmitter();
  var listeners = [];

  for (var k = 0; k < 10; k += 1)
    listeners.push(function() {});

  bench.start();
  for (var i = 0; i < n; i += 1) {
    for (var k = listeners.length; --k >= 0; /* empty */)
      ee.once('dummy', listeners[k]);
    for (var k = listeners.length; --k >= 0; /* empty */)
      ee.removeListener('dummy', listeners[k]);
  }
  bench.end(n);
}

var common = require('../common.js');
//var EventEmitter = require('../../lib/events').EventEmitter;
var EventEmitter = require('events').EventEmitter;

var bench = common.createBenchmark(main, {n: [2e7]});

function main(conf) {
  var n = conf.n | 0;

  var ee = new EventEmitter();

  for (var k = 0; k < 10; k += 1)
    ee.once('dummy', function() {});

  bench.start();
  for (var i = 0; i < n; i += 1) {
    ee.emit('dummy');
  }
  bench.end(n);
}

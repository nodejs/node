'use strict';

var common = require('../common');
var EventEmitter = require('events');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  n: [5e7]
});

function main(conf) {
  var n = conf.n | 0;

  var ee = new EventEmitter();

  for (var k = 0; k < 10; k += 1)
    ee.on('dummy', function() {});

  ee.listenerCount('dummy');
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(ee.listenerCount)');
  ee.listenerCount('dummy');

  bench.start();
  for (var i = 0; i < n; i += 1) {
    ee.listenerCount('dummy');
  }
  bench.end(n);
}

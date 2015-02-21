'use strict';

var common = require('../common');
var EventEmitter = require('events');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  n: [100e5]
});

function main(conf) {
  var n = +conf.n;
  var ee = new EventEmitter();
  var listeners = [];

  var k;
  for (k = 0; k < 10; k += 1)
    listeners.push(function() {});

  // Force optimization before starting the benchmark
  ee.on('dummy', listeners[0]);
  ee._events = {};
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(ee.on)');
  ee.on('dummy', listeners[0]);
  ee._events = {};

  bench.start();
  for (var i = 0; i < n; ++i) {
    for (k = listeners.length; --k >= 0; /* empty */)
      ee.on('dummy', listeners[k]);
    ee._events = {};
  }
  bench.end(n);
}

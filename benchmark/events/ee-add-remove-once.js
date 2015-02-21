'use strict';

var common = require('../common');
var EventEmitter = require('events');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  n: [25e6]
});

function main(conf) {
  var n = conf.n | 0;

  var ee = new EventEmitter();
  var listeners = [];

  var k;
  for (k = 0; k < 10; k += 1)
    listeners.push(function() {});

  for (k = listeners.length; --k >= 0; /* empty */)
    ee.once('dummy', listeners[k]);
  for (k = listeners.length; --k >= 0; /* empty */)
    ee.removeListener('dummy', listeners[k]);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(ee.once)');
  eval('%OptimizeFunctionOnNextCall(ee.removeListener)');
  for (k = listeners.length; --k >= 0; /* empty */)
    ee.once('dummy', listeners[k]);
  for (k = listeners.length; --k >= 0; /* empty */)
    ee.removeListener('dummy', listeners[k]);

  bench.start();
  for (var i = 0; i < n; i += 1) {
    for (k = listeners.length; --k >= 0; /* empty */)
      ee.once('dummy', listeners[k]);
    for (k = listeners.length; --k >= 0; /* empty */)
      ee.removeListener('dummy', listeners[k]);
  }
  bench.end(n);
}

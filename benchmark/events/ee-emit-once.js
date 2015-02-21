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

  function noop() {}

  ee.once('dummy', noop);
  ee.emit('dummy');
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(ee.once)');
  eval('%OptimizeFunctionOnNextCall(ee.emit)');
  ee.once('dummy', noop);
  ee.emit('dummy');

  bench.start();
  for (var i = 0; i < n; i += 1) {
    ee.once('dummy', noop);
    ee.emit('dummy');
  }
  bench.end(n);
}

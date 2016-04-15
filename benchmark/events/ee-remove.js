'use strict';

var common = require('../common');
var EventEmitter = require('events');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  n: [20e6]
});

function main(conf) {
  var n = +conf.n;
  var ee = new EventEmitter();
  var listenerCount = 10;

  function noop() {}

  function setListeners() {
    var arr = new Array(listenerCount);
    for (var i = 0; i < arr.length; ++i)
      arr[i] = noop;
    arr.onceCount = 0;
    ee._events.dummy = arr;
    ee._eventsCount = 1;
  }

  var k;

  // Force optimization before starting the benchmark
  setListeners();
  for (k = listenerCount; --k >= 0; /* empty */)
    ee.removeListener('dummy', noop);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(ee.removeListener)');
  eval('%OptimizeFunctionOnNextCall(setListeners)');
  setListeners();
  for (k = listenerCount; --k >= 0; /* empty */)
    ee.removeListener('dummy', noop);

  bench.start();
  for (var i = 0; i < n; ++i) {
    setListeners();
    for (k = listenerCount; --k >= 0; /* empty */)
      ee.removeListener('dummy', noop);
  }
  bench.end(n);
}

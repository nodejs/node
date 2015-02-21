'use strict';

var common = require('../common');
var EventEmitter = require('events');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  n: [60e6],
  withType: ['true', 'false']
});

function main(conf) {
  var n = +conf.n;
  var ee = new EventEmitter();

  function noop() {}

  function setListeners() {
    var arr = new Array(10);
    for (var i = 0; i < arr.length; ++i)
      arr[i] = noop;
    arr.onceCount = 0;
    ee._events.dummy = arr;
    ee._eventsCount = 1;
  }

  // Force optimization before starting the benchmark
  setListeners();
  if (conf.withType === 'true')
    ee.removeAllListeners('dummy');
  else
    ee.removeAllListeners();
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(setListeners)');
  eval('%OptimizeFunctionOnNextCall(ee.removeAllListeners)');
  setListeners();
  if (conf.withType === 'true')
    ee.removeAllListeners('dummy');
  else
    ee.removeAllListeners();

  var i;
  if (conf.withType === 'true') {
    bench.start();
    for (i = 0; i < n; ++i) {
      setListeners();
      ee.removeAllListeners('dummy');
    }
    bench.end(n);
  } else {
    bench.start();
    for (i = 0; i < n; ++i) {
      setListeners();
      ee.removeAllListeners();
    }
    bench.end(n);
  }
}

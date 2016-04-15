'use strict';

var common = require('../common');
var EventEmitter = require('events');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  n: [50e6],
  withType: ['true', 'false']
});

function main(conf) {
  var n = +conf.n;
  var ee = new EventEmitter();
  var isNewEE = (function() {
    var ee2 = new EventEmitter();
    ee2.once('foo', noop);
    ee2.once('foo', noop);
    return (typeof ee2._events.foo.onceCount === 'number');
  })();

  function noop() {}

  function onceWrap(listener) {
    var fired = false;
    function g() {
      this.removeListener('dummy', g);
      if (!fired) {
        fired = true;
        listener.apply(this, arguments);
      }
    }
    g.listener = listener;
    return g;
  }

  function setListeners() {
    var arr = new Array(10);
    var i;
    if (isNewEE) {
      for (i = 0; i < arr.length; ++i)
        arr[i] = { once: noop, fired: false };
      arr.onceCount = arr.length;
    } else {
      for (i = 0; i < arr.length; ++i)
        arr[i] = onceWrap(noop);
    }
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
  eval('%OptimizeFunctionOnNextCall(ee.removeAllListeners)');
  eval('%OptimizeFunctionOnNextCall(setListeners)');
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


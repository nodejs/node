'use strict';
var common = require('../common.js');
var EventEmitter = require('events').EventEmitter;

var bench = common.createBenchmark(main, { n: [2e7] });

function main(conf) {
  var n = conf.n | 0;

  var ee = new EventEmitter();

  function listener() {}

  bench.start();
  for (var i = 0; i < n; i += 1) {
    var dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
    ee.once(dummy, listener);
    ee.emit(dummy);
  }
  bench.end(n);
}

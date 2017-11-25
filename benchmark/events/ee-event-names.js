'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, { n: [1e6] });

function main(conf) {
  const n = conf.n | 0;

  const ee = new EventEmitter();

  for (var k = 0; k < 9; k += 1) {
    ee.on(`dummy0${k}`, function() {});
    ee.on(`dummy1${k}`, function() {});
    ee.on(`dummy2${k}`, function() {});
  }

  ee.removeAllListeners('dummy01');
  ee.removeAllListeners('dummy11');
  ee.removeAllListeners('dummy21');
  ee.removeAllListeners('dummy06');
  ee.removeAllListeners('dummy16');
  ee.removeAllListeners('dummy26');

  bench.start();
  for (var i = 0; i < n; i += 1) {
    ee.eventNames();
  }
  bench.end(n);
}

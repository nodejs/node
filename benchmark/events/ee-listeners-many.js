'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, { n: [5e6] });

function main({ n }) {
  const ee = new EventEmitter();
  ee.setMaxListeners(101);

  for (var k = 0; k < 50; k += 1) {
    ee.on('dummy0', function() {});
    ee.on('dummy1', function() {});
  }

  bench.start();
  for (var i = 0; i < n; i += 1) {
    const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
    ee.listeners(dummy);
  }
  bench.end(n);
}

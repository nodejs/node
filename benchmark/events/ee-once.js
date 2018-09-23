'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, { n: [2e7] });

function main({ n }) {
  const ee = new EventEmitter();

  function listener() {}

  bench.start();
  for (var i = 0; i < n; i += 1) {
    const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
    ee.once(dummy, listener);
    ee.emit(dummy);
  }
  bench.end(n);
}

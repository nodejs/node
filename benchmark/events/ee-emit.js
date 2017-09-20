'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, { n: [2e6] });

function main(conf) {
  const n = conf.n | 0;

  const ee = new EventEmitter();

  for (var k = 0; k < 10; k += 1)
    ee.on('dummy', function() {});

  bench.start();
  for (var i = 0; i < n; i += 1) {
    ee.emit('dummy');
  }
  bench.end(n);
}

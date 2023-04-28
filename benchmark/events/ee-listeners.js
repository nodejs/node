'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, {
  n: [5e6],
  listeners: [5, 50],
  raw: ['true', 'false'],
});

function main({ n, listeners, raw }) {
  const ee = new EventEmitter();
  ee.setMaxListeners(listeners * 2 + 1);

  for (let k = 0; k < listeners; k += 1) {
    ee.on('dummy0', () => {});
    ee.once('dummy1', () => {});
  }

  if (raw === 'true') {
    bench.start();
    for (let i = 0; i < n; i += 1) {
      const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
      ee.rawListeners(dummy);
    }
    bench.end(n);
  } else {
    bench.start();
    for (let i = 0; i < n; i += 1) {
      const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
      ee.listeners(dummy);
    }
    bench.end(n);
  }
}

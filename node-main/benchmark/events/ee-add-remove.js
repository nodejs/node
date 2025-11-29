'use strict';
const common = require('../common.js');
const { EventEmitter } = require('events');

const bench = common.createBenchmark(main, {
  newListener: [0, 1],
  removeListener: [0, 1],
  n: [1e6],
});

function main({ newListener, removeListener, n }) {
  const ee = new EventEmitter();
  const listeners = [];

  for (let k = 0; k < 10; k += 1)
    listeners.push(() => {});

  if (newListener === 1)
    ee.on('newListener', (event, listener) => {});

  if (removeListener === 1)
    ee.on('removeListener', (event, listener) => {});

  bench.start();
  for (let i = 0; i < n; i += 1) {
    const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
    for (let k = listeners.length; --k >= 0; /* empty */) {
      ee.on(dummy, listeners[k]);
    }
    for (let k = listeners.length; --k >= 0; /* empty */) {
      ee.removeListener(dummy, listeners[k]);
    }
  }
  bench.end(n);
}

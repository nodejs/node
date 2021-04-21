'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;
const MathRandom = Math.random;

const bench = common.createBenchmark(main, { n: [1e6] });

function main({ n }) {
  const ee = new EventEmitter();
  const listeners = [];

  for (let i = 0; i < 10; i += 1) {
    const listener = () => {};
    if ((MathRandom() + .5) >= 1) listener.listener = () => {};
    listeners.push(listener);
  }

  ee.on('newListener', () => {});

  bench.start();
  for (let i = 0; i < n; i += 1) {
    for (let k = 0; k < listeners.length; k += 1)
      ee.emit('newListener', listeners[k].listener ?? listeners[k]);
  }
  bench.end(n);
}

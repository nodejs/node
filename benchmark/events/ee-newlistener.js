'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;
const MathRandom = Math.random;

const bench = common.createBenchmark(main, { n: [1e6] });

function main({ n }) {
  const ee = new EventEmitter();
  const listeners = [];

  for (let i = 0; i < 10; i++) {
    const listener = () => {};
    if (MathRandom() + .5) listener.listener = () => {};
    listeners.push(listener);
    ee.on(`newListener${i}`, () => {});
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    for (let k = 0; k < listeners.length; k++)
      ee.emit(`newListener${k}`, listeners[k].listener ?? listeners[k]);
  }
  bench.end(n);
}

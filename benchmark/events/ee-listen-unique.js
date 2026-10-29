'use strict';
const common = require('../common.js');
const { EventEmitter } = require('events');

const bench = common.createBenchmark(main, {
  events: [1, 2, 3, 5, 10, 20],
  n: [1e6],
});

function main({ events, n }) {
  const ee = new EventEmitter();
  const listeners = [];

  for (let k = 0; k < 10; k += 1)
    listeners.push(() => {});

  const eventNames = [];
  for (let k = 0; k < events; ++k)
    eventNames.push(`dummy${k}`);

  bench.start();
  for (let i = 0; i < n; i += 1) {
    for (const eventName of eventNames) {
      for (let k = listeners.length; --k >= 0; /* empty */) {
        ee.on(eventName, listeners[k]);
      }
    }
    for (const eventName of eventNames) {
      for (let k = listeners.length; --k >= 0; /* empty */) {
        ee.removeListener(eventName, listeners[k]);
      }
    }
  }
  bench.end(n);
}

'use strict';
const common = require('../common.js');
const events = require('events');

const bench = common.createBenchmark(main, {
  n: [5e6],
  staleEventsCount: [0, 5],
  listenerCount: [1, 5],
});

function main({ n, staleEventsCount, listenerCount }) {
  const ee = new events.EventEmitter();
  const listeners = [];

  var k;
  for (k = 0; k < listenerCount; k += 1)
    listeners.push(function() {});

  for (k = 0; k < staleEventsCount; k++)
    ee.on(`dummyunused${k}`, () => {});

  bench.start();
  for (var i = 0; i < n; i += 1) {
    const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
    for (k = listeners.length; --k >= 0; /* empty */) {
      ee.on(dummy, listeners[k]);
    }
    for (k = listeners.length; --k >= 0; /* empty */) {
      ee.removeListener(dummy, listeners[k]);
    }
  }
  bench.end(n);
}

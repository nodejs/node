'use strict';
const common = require('../common.js');
const events = require('events');

const bench = common.createBenchmark(main, { n: [1e6] });

function main({ n }) {
  const ee = new events.EventEmitter();
  const listeners = [];

  var k;
  for (k = 0; k < 10; k += 1)
    listeners.push(() => {});

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

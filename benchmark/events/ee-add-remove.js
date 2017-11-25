'use strict';
const common = require('../common.js');
const events = require('events');

const bench = common.createBenchmark(main, {
  n: [5e6],
  events: [0, 5],
  listeners: [1, 5]
});

function main(conf) {
  let n = conf.n | 0;
  const eventsCOunt = conf.events | 0;
  const listenersCount = conf.listeners | 0;

  const ee = new events.EventEmitter();
  const listeners = [];

  if (listenersCount === 1)
    n *= 2;

  var k;
  for (k = 0; k < listenersCount; k += 1)
    listeners.push(function() {});

  for (k = 0; k < eventsCOunt; k++)
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

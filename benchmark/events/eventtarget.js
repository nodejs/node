'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e6],
  listeners: [1, 5, 10],
}, { flags: ['--expose-internals'] });

function main({ n, listeners }) {
  const { EventTarget, Event } = require('internal/event_target');
  const target = new EventTarget();

  for (let n = 0; n < listeners; n++)
    target.addEventListener('foo', () => {});

  bench.start();
  for (let i = 0; i < n; i++) {
    target.dispatchEvent(new Event('foo'));
  }
  bench.end(n);

}

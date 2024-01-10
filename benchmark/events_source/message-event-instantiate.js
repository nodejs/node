'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [100000],
}, {
  flags: ['--expose-internals'],
});

function main({ n }) {
  const { MessageEvent } = require('internal/event_source');

  bench.start();

  for (let i = 0; i < n; i++) {
    new MessageEvent('message', { });
  }

  bench.end(n);
}

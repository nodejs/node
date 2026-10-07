'use strict';

const common = require('../common.js');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');

const bench = common.createBenchmark(main, {
  n: [1e7],
  type: ['tracing', 'bounded'],
  subscribers: [0, 1],
});

function noop() {}

function main({ n, type, subscribers }) {
  const channel = type === 'tracing' ?
    dc.tracingChannel('test') :
    dc.boundedChannel('test');

  if (subscribers) {
    channel.end.subscribe(noop);
  }

  let noDead;
  bench.start();
  for (let i = 0; i < n; i++) {
    noDead = channel.hasSubscribers;
  }
  bench.end(n);
  assert.strictEqual(noDead, subscribers === 1);
}

'use strict';

const common = require('../common.js');
const dc = require('node:diagnostics_channel');

const bench = common.createBenchmark(main, {
  n: [1e7],
  context: ['omitted', 'undefined', 'provided'],
  subscribers: [0, 1],
});

function noop() {}

function main({ n, context, subscribers }) {
  const channel = dc.tracingChannel('test');
  const providedContext = { __proto__: null };

  if (subscribers) {
    channel.subscribe({ start: noop });
  }

  bench.start();
  switch (context) {
    case 'omitted':
      for (let i = 0; i < n; i++) {
        channel.traceSync(noop);
      }
      break;
    case 'undefined':
      for (let i = 0; i < n; i++) {
        channel.traceSync(noop, undefined);
      }
      break;
    case 'provided':
      for (let i = 0; i < n; i++) {
        channel.traceSync(noop, providedContext);
      }
      break;
    default:
      throw new Error(`Unsupported context value: ${context}`);
  }
  bench.end(n);
}

'use strict';

const common = require('../common.js');
const dc = require('node:diagnostics_channel');

const bench = common.createBenchmark(main, {
  n: [1e7],
  context: ['omitted', 'undefined', 'provided'],
  subscribers: [0, 1],
});

function noop() {}

const thenable = {
  then(onResolve) {
    onResolve(undefined);
  },
};

function returnThenable() {
  return thenable;
}

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
        channel.tracePromise(returnThenable);
      }
      break;
    case 'undefined':
      for (let i = 0; i < n; i++) {
        channel.tracePromise(returnThenable, undefined);
      }
      break;
    case 'provided':
      for (let i = 0; i < n; i++) {
        channel.tracePromise(returnThenable, providedContext);
      }
      break;
    default:
      throw new Error(`Unsupported context value: ${context}`);
  }
  bench.end(n);
}

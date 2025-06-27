'use strict';
const common = require('../common.js');
const { createHook, AsyncResource } = require('async_hooks');

const bench = common.createBenchmark(main, {
  n: [1e6],
  method: [
    'trackingEnabled',
    'trackingEnabledWithDestroyHook',
    'trackingDisabled',
  ],
}, {
  flags: ['--expose-gc'],
});

function endAfterGC(n) {
  setImmediate(() => {
    global.gc();
    setImmediate(() => {
      bench.end(n);
    });
  });
}

function main({ n, method }) {
  switch (method) {
    case 'trackingEnabled':
      bench.start();
      for (let i = 0; i < n; i++) {
        new AsyncResource('foobar');
      }
      endAfterGC(n);
      break;
    case 'trackingEnabledWithDestroyHook':
      createHook({ destroy: () => {} }).enable();
      bench.start();
      for (let i = 0; i < n; i++) {
        new AsyncResource('foobar');
      }
      endAfterGC(n);
      break;
    case 'trackingDisabled':
      bench.start();
      for (let i = 0; i < n; i++) {
        new AsyncResource('foobar', { requireManualDestroy: true });
      }
      endAfterGC(n);
      break;
    default:
      throw new Error(`Unsupported method "${method}"`);
  }
}

'use strict';

const { bench } = require('node:bench');

if (globalThis.benchRunnerPreloaded !== true) {
  throw new Error('benchmark preload did not run');
}

bench('preloaded', {
  params: { preloaded: true },
  samples: 1,
}, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

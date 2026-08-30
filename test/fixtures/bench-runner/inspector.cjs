'use strict';

const { bench } = require('node:bench');

const inspectPort = process.execArgv.filter(
  (arg) => arg.startsWith('--inspect-port=')).at(-1);

bench('inspector option', {
  params: { inspectPort },
  samples: 1,
}, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});

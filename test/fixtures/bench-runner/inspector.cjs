'use strict';

const { bench } = require('node:bench');

const inspectPort = process.execArgv.filter(
  (arg) => arg.startsWith('--inspect-port=')).at(-1);

bench('inspector option', {
  params: { inspectPort },
  samples: 1,
}, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

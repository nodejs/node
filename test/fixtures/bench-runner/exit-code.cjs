'use strict';

const { bench } = require('node:bench');

bench('sets exit code', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
  process.exitCode = 2;
});

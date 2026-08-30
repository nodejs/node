'use strict';

const { bench } = require('node:bench');

bench('many records', { samples: 30 }, (b) => {
  process.stdout.write(`${b.index}\n`);
  b.record({ duration_ns: 1n, operations: 1 });
});

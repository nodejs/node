'use strict';

const { bench } = require('node:bench');

process.stdout.write('benchmark stdout\n');
process.stderr.write('benchmark stderr\n');

bench('output', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});

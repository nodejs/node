'use strict';

const { bench } = require('node:bench');

process.stdout.write('benchmark stdout\n');
process.stderr.write('benchmark stderr\n');

bench('output', { samples: 1 }, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

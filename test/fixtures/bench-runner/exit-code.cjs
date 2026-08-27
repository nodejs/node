'use strict';

const { bench } = require('node:bench');

bench('sets exit code', { samples: 1 }, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
  process.exitCode = 2;
});

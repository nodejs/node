'use strict';

const { bench } = require('node:bench');

bench('tools/no-params.js', (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

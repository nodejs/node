'use strict';

const { bench } = require('node:bench');

bench('tools/reserved.js', { params: { rate: 'parameter' } }, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

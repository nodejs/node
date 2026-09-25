'use strict';

const { bench } = require('node:bench');

bench('tools/reserved.js', { params: { rate: 'parameter' } }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});

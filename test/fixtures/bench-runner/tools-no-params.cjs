'use strict';

const { bench } = require('node:bench');

bench('tools/no-params.js', (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});

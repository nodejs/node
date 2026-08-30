'use strict';

const { bench, run } = require('node:bench');

bench('declared before run', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});

run();

'use strict';

const { bench, run } = require('node:bench');

bench('declared before run', { samples: 1 }, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

run();

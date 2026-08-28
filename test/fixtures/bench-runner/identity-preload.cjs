'use strict';

const { bench } = require('node:bench');

bench('preload identity', { samples: 1 }, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

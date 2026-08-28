'use strict';

const { bench } = require('node:bench');

bench('declared before load error', { samples: 1 }, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

throw new Error('load failed after declaration');

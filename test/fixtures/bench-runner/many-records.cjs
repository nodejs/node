'use strict';

const { bench } = require('node:bench');

bench('many records', { samples: 30 }, (b) => {
  process.stdout.write(`${b.index}\n`);
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

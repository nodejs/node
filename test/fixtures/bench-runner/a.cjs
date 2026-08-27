'use strict';

const { bench } = require('node:bench');

bench('alpha', {
  params: { file: 'a', pid: process.pid },
  samples: 1,
}, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

'use strict';

const { bench } = require('node:bench');

bench('alpha', {
  params: { file: 'a', pid: process.pid },
  samples: 1,
}, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});

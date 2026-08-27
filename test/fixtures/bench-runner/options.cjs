'use strict';

const { bench } = require('node:bench');

let invocations = 0;
bench('selected', {
  params: { boolean: true, number: 42, string: 'value' },
  samples: 8,
  warmup: 8,
}, (b) => {
  invocations++;
  b.start();
  process.hrtime.bigint();
  b.end(invocations);
});

bench('filtered out', { samples: 1 }, () => {
  throw new Error('filtered benchmark ran');
});

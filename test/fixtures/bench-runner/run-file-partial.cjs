'use strict';

const { bench } = require('node:bench');

bench('completed before abort', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});

bench('aborted run file', { samples: 1 }, async () => {
  await new Promise(() => {});
});

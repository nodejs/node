// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { bench, run } = require('node:bench');

let invocations = 0;
bench('overridden', { samples: 8, warmup: 8 }, (b) => {
  invocations++;
  b.start();
  process.hrtime.bigint();
  b.end(invocations);
});

const samples = [];
const stream = run({ samples: 2, warmup: 3 });
stream.on('bench:sample', ({ operations }) => samples.push(operations));
stream.on('end', common.mustCall(() => {
  assert.deepStrictEqual(samples, [4, 5]);
}));
stream.resume();

'use strict';

const assert = require('assert');
const { bench } = require('node:bench');

assert.strictEqual(Error.stackTraceLimit, 17);
assert(process.execArgv.includes('--random-seed=17'));

bench('V8 option', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});

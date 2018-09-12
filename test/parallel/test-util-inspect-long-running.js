'use strict';
const common = require('../common');

// `util.inspect` is capable of computing a string that is bigger than 2 ** 28
// in one second, so let's just skip this test on 32bit systems.
common.skipIf32Bits();

// Test that big objects are running only up to the maximum complexity plus ~1
// second.

const util = require('util');

// Create a difficult to stringify object. Without the limit this would crash.
let last = {};
const obj = last;

for (let i = 0; i < 1000; i++) {
  last.next = { circular: obj, last, obj: { a: 1, b: 2, c: true } };
  last = last.next;
  obj[i] = last;
}

common.expectWarning(
  'Warning',
  'util.inspect took to long.',
  'INSPECTION_ABORTED'
);

util.inspect(obj, { depth: Infinity, budget: 1e6 });

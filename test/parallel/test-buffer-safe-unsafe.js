'use strict';

require('../common');
const assert = require('assert');

const safe = Buffer.alloc(10);

function isZeroFilled(buf) {
  for (let n = 0; n < buf.length; n++)
    if (buf[n] !== 0) return false;
  return true;
}

assert(isZeroFilled(safe));

// Test that unsafe allocations doesn't affect subsequent safe allocations
Buffer.allocUnsafe(10);
assert(isZeroFilled(new Float64Array(10)));

new Buffer(10);
assert(isZeroFilled(new Float64Array(10)));

Buffer.allocUnsafe(10);
assert(isZeroFilled(Buffer.alloc(10)));

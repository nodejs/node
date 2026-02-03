'use strict';

// This tests that Buffer.prototype.toString() works with buffers over 4GB.
const common = require('../common');

// Must not throw when start and end are within kMaxLength
// Cannot test on 32bit machine as we are testing the case
// when start and end are above the threshold
common.skipIf32Bits();
const threshold = 0xFFFFFFFF; // 2^32 - 1
let largeBuffer;
try {
  largeBuffer = Buffer.alloc(threshold + 20);
} catch (e) {
  if (e.code === 'ERR_MEMORY_ALLOCATION_FAILED' || /Array buffer allocation failed/.test(e.message)) {
    common.skip('insufficient space for Buffer.alloc');
  }
  throw e;
}
largeBuffer.toString('utf8', threshold, threshold + 20);

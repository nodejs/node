// Flags: --expose-internals --zero-fill-buffers
// Verifies that Buffer.allocUnsafe() allocates initialized memory under --zero-fill-buffers by
// checking the usage count of the relevant native allocator code path.
'use strict';

const common = require('../common');
if (!common.isDebug) {
  common.skip('Only works in debug mode');
}
const { internalBinding } = require('internal/test/binding');
const { getGenericUsageCount } = internalBinding('debug');
const assert = require('assert');

const initialCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.ZeroFilled');
const buffer = Buffer.allocUnsafe(Buffer.poolSize + 1);
assert.strictEqual(buffer.every((b) => b === 0), true);
const newCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.ZeroFilled');
assert.notStrictEqual(newCount, initialCount);

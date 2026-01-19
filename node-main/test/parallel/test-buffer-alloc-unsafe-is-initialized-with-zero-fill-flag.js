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

const initialUninitializedCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.Uninitialized');
const initialZeroFilledCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.ZeroFilled');
const buffer = Buffer.allocUnsafe(Buffer.poolSize + 1);
assert(buffer.every((b) => b === 0));
const newUninitializedCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.Uninitialized');
const newZeroFilledCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.ZeroFilled');
assert.strictEqual(newUninitializedCount, initialUninitializedCount);
assert.notStrictEqual(newZeroFilledCount, initialZeroFilledCount);

// Flags: --expose-internals
// Verifies that Buffer.allocUnsafe() indeed allocates uninitialized memory by checking
// the usage count of the relevant native allocator code path.
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
Buffer.allocUnsafe(Buffer.poolSize + 1);
// We can't reliably check the contents of the buffer here because the OS or memory allocator
// used might zero-fill memory for us, or they might happen to return reused memory that was
// previously used by a process that zero-filled it. So instead we just check the usage counts.
const newUninitializedCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.Uninitialized');
const newZeroFilledCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.ZeroFilled');
assert.notStrictEqual(newUninitializedCount, initialUninitializedCount);
assert.strictEqual(newZeroFilledCount, initialZeroFilledCount);

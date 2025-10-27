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

const initialCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.Uninitialized');
Buffer.allocUnsafe(Buffer.poolSize + 1);
// We can't reliably check the contents of the buffer here because the OS or memory allocator
// used might zero-fill memory for us, or they might happen to return reused memory that was
// previously used by a process that zero-filled it.
const newCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.Uninitialized');
assert.notStrictEqual(newCount, initialCount);

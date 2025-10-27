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
const newCount = getGenericUsageCount('NodeArrayBufferAllocator.Allocate.Uninitialized');
assert.notStrictEqual(newCount, initialCount);

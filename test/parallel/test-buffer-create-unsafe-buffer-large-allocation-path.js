// Flags: --expose-internals
'use strict';

const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { loadBuiltinModule } = require('internal/modules/helpers');

const binding = internalBinding('buffer');
const internalBufferModule = loadBuiltinModule('internal/buffer');

function reloadInternalBuffer() {
  internalBufferModule.loaded = false;
  internalBufferModule.loading = false;
  internalBufferModule.exports = {};
  internalBufferModule.exportKeys = undefined;
  internalBufferModule.module = undefined;
  internalBufferModule.compileForPublicLoader();
  return internalBufferModule.exports;
}

const originalCreateUnsafeArrayBuffer = binding.createUnsafeArrayBuffer;
const originalCreateUnsafeBuffer = binding.createUnsafeBuffer;

// Regression: Large allocations should not route through createUnsafeArrayBuffer.
binding.createUnsafeArrayBuffer = () => {
  throw new Error('createUnsafeArrayBuffer should not be called for large buffers');
};

let internalBuffer = reloadInternalBuffer();
const large = internalBuffer.createUnsafeBuffer(65);
assert(large instanceof Uint8Array);
assert.strictEqual(large.length, 65);

// Edge case: Small sizes should remain on the in-heap path.
binding.createUnsafeBuffer = () => {
  throw new Error('createUnsafeBuffer should not be called for small buffers');
};

internalBuffer = reloadInternalBuffer();
const small = internalBuffer.createUnsafeBuffer(64);
assert(small instanceof Uint8Array);
assert.strictEqual(small.length, 64);

// Nearby-path safety check: Public API behavior is unchanged for large sizes.
const userVisible = Buffer.allocUnsafeSlow(65);
assert(Buffer.isBuffer(userVisible));
assert.strictEqual(userVisible.length, 65);

binding.createUnsafeArrayBuffer = originalCreateUnsafeArrayBuffer;
if (originalCreateUnsafeBuffer === undefined) {
  delete binding.createUnsafeBuffer;
} else {
  binding.createUnsafeBuffer = originalCreateUnsafeBuffer;
}
reloadInternalBuffer();

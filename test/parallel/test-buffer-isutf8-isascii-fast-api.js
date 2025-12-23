// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

// Get direct access to the buffer validation methods
const buffer = require('buffer');

// Create test buffers
const utf8Buffer = Buffer.from('Hello, 世界!');  // Valid UTF-8 with actual Unicode characters
const asciiBuffer = Buffer.from('Hello, World!');  // Valid ASCII
const nonUtf8Buffer = Buffer.from([0xFF, 0xFF, 0xFF]); // Invalid UTF-8
const nonAsciiBuffer = Buffer.from([0x80, 0x90, 0xA0]); // Invalid ASCII

// Test basic functionality for isUtf8
assert.strictEqual(buffer.isUtf8(utf8Buffer), true);
assert.strictEqual(buffer.isUtf8(nonUtf8Buffer), false);

// Test basic functionality for isAscii
assert.strictEqual(buffer.isAscii(asciiBuffer), true);
assert.strictEqual(buffer.isAscii(nonAsciiBuffer), false);

// Test detached buffers
const detachedBuffer = new ArrayBuffer(10);
// Let's detach the buffer if it's supported
detachedBuffer.detach?.();

if (detachedBuffer.detached) {
  const typedArray = new Uint8Array(detachedBuffer);

  assert.throws(() => {
    buffer.isUtf8(typedArray);
  }, {
    name: 'Error',
    code: 'ERR_INVALID_STATE'
  });

  assert.throws(() => {
    buffer.isAscii(typedArray);
  }, {
    name: 'Error',
    code: 'ERR_INVALID_STATE'
  });
}

// Test optimization and fast API paths
function testFastPaths() {
  // Test both valid and invalid cases to ensure both paths are optimized
  buffer.isUtf8(utf8Buffer);
  buffer.isUtf8(nonUtf8Buffer);
  buffer.isAscii(asciiBuffer);
  buffer.isAscii(nonAsciiBuffer);
}

// Since we want to optimize the C++ methods, we need to prepare them
// through their JS wrappers
eval('%PrepareFunctionForOptimization(buffer.isUtf8)');
eval('%PrepareFunctionForOptimization(buffer.isAscii)');
testFastPaths();
eval('%OptimizeFunctionOnNextCall(buffer.isUtf8)');
eval('%OptimizeFunctionOnNextCall(buffer.isAscii)');
testFastPaths();

// Verify fast API calls were made if running in debug mode
if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('buffer.isUtf8'), 2); // Called twice in testFastPaths
  assert.strictEqual(getV8FastApiCallCount('buffer.isAscii'), 2); // Called twice in testFastPaths
}

// Flags: --permission --allow-fs-read=* --allow-fs-write=* --allow-natives-syntax --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const path = require('path');

// Test that process.permission.has() uses the V8 fast API path.

// Test with scope only (no resource argument).
function testHasScope() {
  assert.strictEqual(process.permission.has('fs.read', __filename), true);
  assert.strictEqual(process.permission.has('fs.write', __dirname), true);
  assert.strictEqual(
    process.permission.has('fs.read', path.resolve('/nonexistent')),
    true
  );
  assert.strictEqual(process.permission.has('fs.read'), true);
  assert.strictEqual(process.permission.has('fs.write'), true);
  assert.strictEqual(process.permission.has('child'), false);
  assert.strictEqual(process.permission.has('worker'), false);
  assert.strictEqual(process.permission.has('invalid-key'), false);
}

// Warm up and optimize for the fast API path.
eval('%PrepareFunctionForOptimization(testHasScope)');
testHasScope();
testHasScope();

eval('%OptimizeFunctionOnNextCall(testHasScope)');
testHasScope();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  // After optimization: testHasScope = 4, testHasResource = 3, testHasInvalid = 1
  assert.strictEqual(getV8FastApiCallCount('permission.has'), 8);
}

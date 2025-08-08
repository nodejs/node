'use strict';

const common = require('../../common');
const assert = require('assert');
const test_sharedarraybuffer = require(`./build/${common.buildType}/test_sharedarraybuffer`);

{
  const sab = new SharedArrayBuffer(16);
  const ab = new ArrayBuffer(16);
  const obj = {};
  const arr = [];

  assert.strictEqual(test_sharedarraybuffer.TestIsSharedArrayBuffer(sab), true);
  assert.strictEqual(test_sharedarraybuffer.TestIsSharedArrayBuffer(ab), false);
  assert.strictEqual(test_sharedarraybuffer.TestIsSharedArrayBuffer(obj), false);
  assert.strictEqual(test_sharedarraybuffer.TestIsSharedArrayBuffer(arr), false);
  assert.strictEqual(test_sharedarraybuffer.TestIsSharedArrayBuffer(null), false);
  assert.strictEqual(test_sharedarraybuffer.TestIsSharedArrayBuffer(undefined), false);
}

// Test node_api_create_sharedarraybuffer
{
  const sab = test_sharedarraybuffer.TestCreateSharedArrayBuffer(16);
  assert(sab instanceof SharedArrayBuffer);
  assert.strictEqual(sab.byteLength, 16);
}

// Test node_api_create_get_sharedarraybuffer_info
{
  const sab = new SharedArrayBuffer(32);
  const byteLength = test_sharedarraybuffer.TestGetSharedArrayBufferInfo(sab);
  assert.strictEqual(byteLength, 32);
}

// Test data access
{
  const sab = new SharedArrayBuffer(8);
  const result = test_sharedarraybuffer.TestSharedArrayBufferData(sab);
  assert.strictEqual(result, true);

  // Check if data was written correctly
  const view = new Uint8Array(sab);
  for (let i = 0; i < 8; i++) {
    assert.strictEqual(view[i], i % 256);
  }
}

// Test data pointer from existing SharedArrayBuffer
{
  const sab = new SharedArrayBuffer(16);
  const result = test_sharedarraybuffer.TestSharedArrayBufferData(sab);
  assert.strictEqual(result, true);
}

// Test zero-length SharedArrayBuffer
{
  const sab = test_sharedarraybuffer.TestCreateSharedArrayBuffer(0);
  assert(sab instanceof SharedArrayBuffer);
  assert.strictEqual(sab.byteLength, 0);
}

// Test invalid arguments
{
  assert.throws(() => {
    test_sharedarraybuffer.TestGetSharedArrayBufferInfo({});
  }, { name: 'Error', message: 'Invalid argument' });
}

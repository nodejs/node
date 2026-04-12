'use strict';

// Verify SharedArrayBuffer-backed typed arrays can be created through
// napi_create_typedarray() while preserving existing ArrayBuffer behavior.

const common = require('../../common');
const assert = require('assert');

const test_typedarray_sharedarraybuffer =
  require(`./build/${common.buildType}/test_typedarray_sharedarraybuffer`);

// Test for creating Uint8Array with ArrayBuffer
{
  const buffer = new ArrayBuffer(16);
  const theArray =
    test_typedarray_sharedarraybuffer.CreateUint8Array(buffer, 4, 6);
  const theArrayBuffer =
    test_typedarray_sharedarraybuffer.GetArrayBuffer(theArray);

  assert.ok(theArray instanceof Uint8Array);
  assert.strictEqual(theArray.buffer, buffer);
  assert.ok(theArrayBuffer instanceof ArrayBuffer);
  assert.strictEqual(theArrayBuffer, buffer);
  assert.strictEqual(theArray.byteOffset, 4);
  assert.strictEqual(theArray.length, 6);

  theArray.set([1, 2, 3, 4, 5, 6]);
  assert.deepStrictEqual(Array.from(new Uint8Array(buffer, 4, 6)),
                         [1, 2, 3, 4, 5, 6]);
}

// Test for creating Uint8Array with SharedArrayBuffer
{
  const buffer = new SharedArrayBuffer(16);
  const theArray =
    test_typedarray_sharedarraybuffer.CreateUint8Array(buffer, 4, 6);
  const theArrayBuffer =
    test_typedarray_sharedarraybuffer.GetArrayBuffer(theArray);

  assert.ok(theArray instanceof Uint8Array);
  assert.strictEqual(theArray.buffer, buffer);
  assert.ok(theArrayBuffer instanceof SharedArrayBuffer);
  assert.strictEqual(theArrayBuffer, buffer);
  assert.strictEqual(theArray.byteOffset, 4);
  assert.strictEqual(theArray.length, 6);

  theArray.set([6, 5, 4, 3, 2, 1]);
  assert.deepStrictEqual(Array.from(new Uint8Array(buffer, 4, 6)),
                         [6, 5, 4, 3, 2, 1]);
}

// Test for creating Uint16Array with SharedArrayBuffer
{
  const buffer = new SharedArrayBuffer(24);
  const theArray =
    test_typedarray_sharedarraybuffer.CreateUint16Array(buffer, 4, 4);

  assert.ok(theArray instanceof Uint16Array);
  assert.strictEqual(theArray.buffer, buffer);
  assert.strictEqual(theArray.byteOffset, 4);
  assert.strictEqual(theArray.length, 4);

  theArray.set([1, 2, 3, 65535]);
  assert.deepStrictEqual(Array.from(new Uint16Array(buffer, 4, 4)),
                         [1, 2, 3, 65535]);
}

// Test for creating Int32Array with SharedArrayBuffer
{
  const buffer = new SharedArrayBuffer(32);
  const theArray =
    test_typedarray_sharedarraybuffer.CreateInt32Array(buffer, 8, 3);

  assert.ok(theArray instanceof Int32Array);
  assert.strictEqual(theArray.buffer, buffer);
  assert.strictEqual(theArray.byteOffset, 8);
  assert.strictEqual(theArray.length, 3);

  theArray.set([-1, 0, 123456789]);
  assert.deepStrictEqual(Array.from(new Int32Array(buffer, 8, 3)),
                         [-1, 0, 123456789]);
}

// Test for creating TypedArrays with SharedArrayBuffer and invalid range
{
  const buffer = new SharedArrayBuffer(8);

  assert.throws(() => {
    test_typedarray_sharedarraybuffer.CreateUint8Array(buffer, 9, 0);
  }, RangeError);

  assert.throws(() => {
    test_typedarray_sharedarraybuffer.CreateUint16Array(buffer, 0, 5);
  }, RangeError);

  assert.throws(() => {
    test_typedarray_sharedarraybuffer.CreateUint16Array(buffer, 1, 1);
  }, RangeError);
}

// Test invalid arguments
{
  assert.throws(() => {
    test_typedarray_sharedarraybuffer.CreateUint8Array({}, 0, 1);
  }, { name: 'Error', message: 'Invalid argument' });

  assert.throws(() => {
    test_typedarray_sharedarraybuffer.CreateUint8Array(1, 0, 1);
  }, { name: 'Error', message: 'Invalid argument' });
}

// Flags: --experimental-stream-iter
'use strict';

// Tests for transform output normalization edge cases:
// ArrayBuffer, ArrayBufferView, iterables, strings, invalid types.

const common = require('../common');
const assert = require('assert');
const {
  pull,
  pullSync,
  bytes,
  bytesSync,
  from,
  fromSync,
} = require('stream/iter');

// Stateless transform returns ArrayBuffer (async)
async function testTransformReturnsArrayBuffer() {
  const tx = (chunks) => {
    if (chunks === null) return null;
    const ab = new ArrayBuffer(chunks[0].length);
    new Uint8Array(ab).set(chunks[0]);
    return ab;
  };
  const data = await bytes(pull(from('AB'), tx));
  assert.deepStrictEqual(data, new TextEncoder().encode('AB'));
}

// Stateless transform returns ArrayBuffer (sync)
async function testSyncTransformReturnsArrayBuffer() {
  const tx = (chunks) => {
    if (chunks === null) return null;
    const ab = new ArrayBuffer(chunks[0].length);
    new Uint8Array(ab).set(chunks[0]);
    return ab;
  };
  const data = bytesSync(pullSync(fromSync('AB'), tx));
  assert.deepStrictEqual(data, new TextEncoder().encode('AB'));
}

// Stateless transform returns Float32Array (non-Uint8Array ArrayBufferView)
async function testTransformReturnsFloat32Array() {
  const tx = (chunks) => {
    if (chunks === null) return null;
    return new Float32Array([1.0]);
  };
  const data = await bytes(pull(from('x'), tx));
  assert.strictEqual(data.byteLength, 4); // 1 float32 = 4 bytes
}

// Stateless sync transform returns Float32Array
async function testSyncTransformReturnsFloat32Array() {
  const tx = (chunks) => {
    if (chunks === null) return null;
    return new Float32Array([1.0]);
  };
  const data = bytesSync(pullSync(fromSync('x'), tx));
  assert.strictEqual(data.byteLength, 4);
}

// Stateless transform returns a sync generator (iterable)
async function testTransformReturnsGenerator() {
  const tx = (chunks) => {
    if (chunks === null) return null;
    return (function*() {
      yield new Uint8Array([65]);
      yield new Uint8Array([66]);
    })();
  };
  const data = await bytes(pull(from('x'), tx));
  assert.deepStrictEqual(data, new Uint8Array([65, 66]));
}

// Stateless sync transform returns a generator
async function testSyncTransformReturnsGenerator() {
  const tx = (chunks) => {
    if (chunks === null) return null;
    return (function*() {
      yield new Uint8Array([67]);
      yield new Uint8Array([68]);
    })();
  };
  const data = bytesSync(pullSync(fromSync('x'), tx));
  assert.deepStrictEqual(data, new Uint8Array([67, 68]));
}

// Stateless async transform returns an async generator
async function testTransformReturnsAsyncGenerator() {
  const tx = (chunks) => {
    if (chunks === null) return null;
    return (async function*() {
      yield new Uint8Array([69]);
      yield new Uint8Array([70]);
    })();
  };
  const data = await bytes(pull(from('x'), tx));
  assert.deepStrictEqual(data, new Uint8Array([69, 70]));
}

// Stateful async transform yields string
async function testStatefulTransformYieldsString() {
  const tx = {
    async *transform(source) {
      for await (const chunks of source) {
        if (chunks === null) return;
        yield 'hello';
      }
    },
  };
  const data = await bytes(pull(from('x'), tx));
  assert.deepStrictEqual(data, new TextEncoder().encode('hello'));
}

// Stateful async transform yields ArrayBuffer
async function testStatefulTransformYieldsArrayBuffer() {
  const tx = {
    async *transform(source) {
      for await (const chunks of source) {
        if (chunks === null) return;
        const ab = new ArrayBuffer(2);
        new Uint8Array(ab).set([71, 72]);
        yield ab;
      }
    },
  };
  const data = await bytes(pull(from('x'), tx));
  assert.deepStrictEqual(data, new Uint8Array([71, 72]));
}

// Stateful sync transform yields string
async function testStatefulSyncTransformYieldsString() {
  const tx = {
    *transform(source) {
      for (const chunks of source) {
        if (chunks === null) return;
        yield 'world';
      }
    },
  };
  const data = bytesSync(pullSync(fromSync('x'), tx));
  assert.deepStrictEqual(data, new TextEncoder().encode('world'));
}

// Flush returns single Uint8Array (not batch)
async function testFlushReturnsSingleUint8Array() {
  const tx = (chunks) => {
    if (chunks === null) return new Uint8Array([99]); // Flush returns single
    return chunks;
  };
  const data = await bytes(pull(from('x'), tx));
  // Should contain both the original data and the flush byte
  assert.ok(data.includes(99));
}

// Flush returns string
async function testFlushReturnsString() {
  const tx = (chunks) => {
    if (chunks === null) return 'trailer';
    return chunks;
  };
  const data = await bytes(pull(from('x'), tx));
  const trailer = new TextEncoder().encode('trailer');
  // Last bytes should be the trailer
  const tail = data.slice(data.length - trailer.length);
  assert.deepStrictEqual(tail, trailer);
}

// Sync flush returns single Uint8Array
async function testSyncFlushReturnsSingleUint8Array() {
  const tx = (chunks) => {
    if (chunks === null) return new Uint8Array([88]);
    return chunks;
  };
  const data = bytesSync(pullSync(fromSync('x'), tx));
  assert.ok(data.includes(88));
}

// Transform returns invalid type → ERR_INVALID_ARG_TYPE
async function testTransformReturnsInvalidType() {
  const tx = (chunks) => {
    if (chunks === null) return null;
    return 42; // Invalid
  };
  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const batch of pull(from('x'), tx)) { /* consume */ }
    },
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

// Sync transform returns invalid type → ERR_INVALID_ARG_TYPE
async function testSyncTransformReturnsInvalidType() {
  const tx = (chunks) => {
    if (chunks === null) return null;
    return 42;
  };
  assert.throws(
    () => {
      // eslint-disable-next-line no-unused-vars
      for (const batch of pullSync(fromSync('x'), tx)) { /* consume */ }
    },
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

Promise.all([
  testTransformReturnsArrayBuffer(),
  testSyncTransformReturnsArrayBuffer(),
  testTransformReturnsFloat32Array(),
  testSyncTransformReturnsFloat32Array(),
  testTransformReturnsGenerator(),
  testSyncTransformReturnsGenerator(),
  testTransformReturnsAsyncGenerator(),
  testStatefulTransformYieldsString(),
  testStatefulTransformYieldsArrayBuffer(),
  testStatefulSyncTransformYieldsString(),
  testFlushReturnsSingleUint8Array(),
  testFlushReturnsString(),
  testSyncFlushReturnsSingleUint8Array(),
  testTransformReturnsInvalidType(),
  testSyncTransformReturnsInvalidType(),
]).then(common.mustCall());

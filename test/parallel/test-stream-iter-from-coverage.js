// Flags: --experimental-stream-iter
'use strict';

// Coverage tests for from.js: sub-batching >128, DataView in generator,
// non-Uint8Array TypedArray normalization.

const common = require('../common');
const assert = require('assert');
const {
  from,
  fromSync,
  bytes,
  bytesSync,
} = require('stream/iter');

// fromSync: Uint8Array[] with > 128 elements triggers sub-batching
async function testFromSyncSubBatching() {
  const bigBatch = Array.from({ length: 200 },
                              (_, i) => new Uint8Array([i & 0xFF]));
  const batches = [];
  for (const batch of fromSync(bigBatch)) {
    batches.push(batch);
  }
  // Should be split into sub-batches: 128 + 72
  assert.strictEqual(batches.length, 2);
  assert.strictEqual(batches[0].length, 128);
  assert.strictEqual(batches[1].length, 72);
  // Verify no data loss
  let totalChunks = 0;
  for (const batch of batches) totalChunks += batch.length;
  assert.strictEqual(totalChunks, 200);
}

// from: Uint8Array[] with > 128 elements triggers sub-batching (async)
async function testFromAsyncSubBatching() {
  const bigBatch = Array.from({ length: 200 },
                              (_, i) => new Uint8Array([i & 0xFF]));
  const batches = [];
  for await (const batch of from(bigBatch)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 2);
  assert.strictEqual(batches[0].length, 128);
  assert.strictEqual(batches[1].length, 72);
}

// Exact boundary: 128 elements → single batch (no split)
async function testFromSubBatchingBoundary() {
  const exactBatch = Array.from({ length: 128 },
                                (_, i) => new Uint8Array([i]));
  const batches = [];
  for (const batch of fromSync(exactBatch)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.strictEqual(batches[0].length, 128);
}

// 129 elements → 2 batches (128 + 1)
async function testFromSubBatchingBoundaryPlus1() {
  const batch129 = Array.from({ length: 129 },
                              (_, i) => new Uint8Array([i & 0xFF]));
  const batches = [];
  for await (const batch of from(batch129)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 2);
  assert.strictEqual(batches[0].length, 128);
  assert.strictEqual(batches[1].length, 1);
}

// DataView yielded from a sync generator → normalizeSyncValue path
async function testFromSyncDataViewInGenerator() {
  function* gen() {
    const buf = new ArrayBuffer(3);
    const dv = new DataView(buf);
    dv.setUint8(0, 65);
    dv.setUint8(1, 66);
    dv.setUint8(2, 67);
    yield dv;
  }
  const data = bytesSync(fromSync(gen()));
  assert.deepStrictEqual(data, new Uint8Array([65, 66, 67]));
}

// DataView yielded from an async generator → normalizeAsyncValue path
async function testFromAsyncDataViewInGenerator() {
  async function* gen() {
    const buf = new ArrayBuffer(3);
    const dv = new DataView(buf);
    dv.setUint8(0, 68);
    dv.setUint8(1, 69);
    dv.setUint8(2, 70);
    yield dv;
  }
  const data = await bytes(from(gen()));
  assert.deepStrictEqual(data, new Uint8Array([68, 69, 70]));
}

// Int16Array yielded from generator → primitiveToUint8Array fallback
async function testFromSyncInt16ArrayInGenerator() {
  function* gen() {
    yield new Int16Array([0x0102, 0x0304]);
  }
  const data = bytesSync(fromSync(gen()));
  assert.strictEqual(data.byteLength, 4); // 2 int16 = 4 bytes
}

// Float64Array as top-level input to from()
async function testFromFloat64Array() {
  const f64 = new Float64Array([1.0]);
  const batches = [];
  for await (const batch of from(f64)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.strictEqual(batches[0][0].byteLength, 8); // 1 float64 = 8 bytes
}

// Sync generator yielding invalid type → ERR_INVALID_ARG_TYPE
async function testFromSyncInvalidYield() {
  function* gen() {
    yield 42; // Not a valid stream value
  }
  assert.throws(
    () => {
      // eslint-disable-next-line no-unused-vars
      for (const batch of fromSync(gen())) { /* consume */ }
    },
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

Promise.all([
  testFromSyncSubBatching(),
  testFromAsyncSubBatching(),
  testFromSubBatchingBoundary(),
  testFromSubBatchingBoundaryPlus1(),
  testFromSyncDataViewInGenerator(),
  testFromAsyncDataViewInGenerator(),
  testFromSyncInt16ArrayInGenerator(),
  testFromFloat64Array(),
  testFromSyncInvalidYield(),
]).then(common.mustCall());

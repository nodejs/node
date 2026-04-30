// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from,
  fromSync,
  bytes,
  bytesSync,
  text,
  textSync,
  arrayBuffer,
  arrayBufferSync,
  pipeTo,
  pipeToSync,
  pull,
  pullSync,
} = require('stream/iter');

// =============================================================================
// from() / fromSync() with SharedArrayBuffer
// =============================================================================

function testFromSyncSAB() {
  const sab = new SharedArrayBuffer(4);
  new Uint8Array(sab).set([10, 20, 30, 40]);
  const batches = [];
  for (const batch of fromSync(sab)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([10, 20, 30, 40]));
}

async function testFromAsyncSAB() {
  const sab = new SharedArrayBuffer(4);
  new Uint8Array(sab).set([10, 20, 30, 40]);
  const batches = [];
  for await (const batch of from(sab)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([10, 20, 30, 40]));
}

// =============================================================================
// Consumers with SAB-backed source
// =============================================================================

function testBytesSyncSAB() {
  const sab = new SharedArrayBuffer(5);
  new Uint8Array(sab).set([104, 101, 108, 108, 111]); // 'hello'
  const data = bytesSync(fromSync(sab));
  assert.deepStrictEqual(data, new Uint8Array([104, 101, 108, 108, 111]));
}

async function testBytesAsyncSAB() {
  const sab = new SharedArrayBuffer(5);
  new Uint8Array(sab).set([104, 101, 108, 108, 111]); // 'hello'
  const data = await bytes(from(sab));
  assert.deepStrictEqual(data, new Uint8Array([104, 101, 108, 108, 111]));
}

function testTextSyncSAB() {
  const sab = new SharedArrayBuffer(5);
  new Uint8Array(sab).set([104, 101, 108, 108, 111]); // 'hello'
  const result = textSync(fromSync(sab));
  assert.strictEqual(result, 'hello');
}

async function testTextAsyncSAB() {
  const sab = new SharedArrayBuffer(5);
  new Uint8Array(sab).set([104, 101, 108, 108, 111]); // 'hello'
  const result = await text(from(sab));
  assert.strictEqual(result, 'hello');
}

function testArrayBufferSyncSAB() {
  const sab = new SharedArrayBuffer(4);
  new Uint8Array(sab).set([1, 2, 3, 4]);
  const result = arrayBufferSync(fromSync(sab));
  assert.ok(result instanceof SharedArrayBuffer || result instanceof ArrayBuffer);
  assert.deepStrictEqual(new Uint8Array(result), new Uint8Array([1, 2, 3, 4]));
}

async function testArrayBufferAsyncSAB() {
  const sab = new SharedArrayBuffer(4);
  new Uint8Array(sab).set([1, 2, 3, 4]);
  const result = await arrayBuffer(from(sab));
  assert.ok(result instanceof SharedArrayBuffer || result instanceof ArrayBuffer);
  assert.deepStrictEqual(new Uint8Array(result), new Uint8Array([1, 2, 3, 4]));
}

// =============================================================================
// pipeTo / pipeToSync with SAB source
// =============================================================================

function testPipeToSyncSAB() {
  const sab = new SharedArrayBuffer(3);
  new Uint8Array(sab).set([65, 66, 67]); // 'ABC'
  const written = [];
  const writer = {
    writeSync(chunk) { written.push(chunk); return true; },
    endSync() { return written.length; },
  };
  const totalBytes = pipeToSync(fromSync(sab), writer);
  assert.strictEqual(totalBytes, 3);
  assert.deepStrictEqual(written[0], new Uint8Array([65, 66, 67]));
}

async function testPipeToAsyncSAB() {
  const sab = new SharedArrayBuffer(3);
  new Uint8Array(sab).set([65, 66, 67]); // 'ABC'
  const written = [];
  const writer = {
    async write(chunk) { written.push(chunk); },
    async end() { return written.length; },
  };
  await pipeTo(from(sab), writer);
  assert.deepStrictEqual(written[0], new Uint8Array([65, 66, 67]));
}

// =============================================================================
// pull / pullSync with SAB-yielding generator
// =============================================================================

function testPullSyncSABChunks() {
  function* gen() {
    const sab = new SharedArrayBuffer(2);
    new Uint8Array(sab).set([1, 2]);
    yield [new Uint8Array(sab)];
  }
  const batches = [];
  for (const batch of pullSync(gen())) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([1, 2]));
}

async function testPullAsyncSABChunks() {
  async function* gen() {
    const sab = new SharedArrayBuffer(2);
    new Uint8Array(sab).set([3, 4]);
    yield [new Uint8Array(sab)];
  }
  const batches = [];
  for await (const batch of pull(gen())) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([3, 4]));
}

// =============================================================================
// Transform returning SAB
// =============================================================================

async function testTransformReturningSAB() {
  function* source() {
    yield [new Uint8Array([1, 2, 3])];
  }
  const transform = (chunks) => {
    if (chunks === null) return null;
    // Transform returns a Uint8Array backed by a SharedArrayBuffer
    const sab = new SharedArrayBuffer(chunks[0].length);
    new Uint8Array(sab).set(chunks[0]);
    return new Uint8Array(sab);
  };
  const batches = [];
  for await (const batch of pull(source(), transform)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([1, 2, 3]));
}

// =============================================================================

Promise.all([
  testFromSyncSAB(),
  testFromAsyncSAB(),
  testBytesSyncSAB(),
  testBytesAsyncSAB(),
  testTextSyncSAB(),
  testTextAsyncSAB(),
  testArrayBufferSyncSAB(),
  testArrayBufferAsyncSAB(),
  testPipeToSyncSAB(),
  testPipeToAsyncSAB(),
  testPullSyncSABChunks(),
  testPullAsyncSABChunks(),
  testTransformReturningSAB(),
]).then(common.mustCall());

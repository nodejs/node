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
  array,
  arraySync,
} = require('stream/iter');

// =============================================================================
// bytesSync / bytes
// =============================================================================

async function testBytesSyncBasic() {
  const data = bytesSync(fromSync('hello'));
  assert.deepStrictEqual(data, new TextEncoder().encode('hello'));
}

async function testBytesSyncLimit() {
  assert.throws(
    () => bytesSync(fromSync('hello world'), { limit: 3 }),
    { name: 'RangeError' },
  );
}

async function testBytesAsync() {
  const data = await bytes(from('hello-async'));
  assert.deepStrictEqual(data, new TextEncoder().encode('hello-async'));
}

async function testBytesAsyncLimit() {
  await assert.rejects(
    () => bytes(from('hello world'), { limit: 3 }),
    { name: 'RangeError' },
  );
}

async function testBytesAsyncAbort() {
  const ac = new AbortController();
  ac.abort();
  await assert.rejects(
    () => bytes(from('data'), { signal: ac.signal }),
    { name: 'AbortError' },
  );
}

async function testBytesEmpty() {
  const data = await bytes(from([]));
  assert.ok(data instanceof Uint8Array);
  assert.strictEqual(data.byteLength, 0);
}

// =============================================================================
// arrayBufferSync / arrayBuffer
// =============================================================================

async function testArrayBufferSyncBasic() {
  const ab = arrayBufferSync(fromSync(new Uint8Array([1, 2, 3])));
  assert.ok(ab instanceof ArrayBuffer);
  assert.strictEqual(ab.byteLength, 3);
  const view = new Uint8Array(ab);
  assert.deepStrictEqual(view, new Uint8Array([1, 2, 3]));
}

async function testArrayBufferAsync() {
  const ab = await arrayBuffer(from(new Uint8Array([10, 20, 30])));
  assert.ok(ab instanceof ArrayBuffer);
  assert.strictEqual(ab.byteLength, 3);
  const view = new Uint8Array(ab);
  assert.deepStrictEqual(view, new Uint8Array([10, 20, 30]));
}

// =============================================================================
// arraySync / array
// =============================================================================

async function testArraySyncBasic() {
  function* gen() {
    yield new Uint8Array([1]);
    yield new Uint8Array([2]);
    yield new Uint8Array([3]);
  }
  const chunks = arraySync(fromSync(gen()));
  assert.strictEqual(chunks.length, 3);
  assert.deepStrictEqual(chunks[0], new Uint8Array([1]));
  assert.deepStrictEqual(chunks[1], new Uint8Array([2]));
  assert.deepStrictEqual(chunks[2], new Uint8Array([3]));
}

async function testArraySyncLimit() {
  function* gen() {
    yield new Uint8Array(100);
    yield new Uint8Array(100);
  }
  const source = fromSync(gen());
  assert.throws(
    () => arraySync(source, { limit: 50 }),
    { name: 'RangeError' },
  );
}

async function testArrayAsync() {
  async function* gen() {
    yield [new Uint8Array([1])];
    yield [new Uint8Array([2])];
  }
  const chunks = await array(gen());
  assert.strictEqual(chunks.length, 2);
  assert.deepStrictEqual(chunks[0], new Uint8Array([1]));
  assert.deepStrictEqual(chunks[1], new Uint8Array([2]));
}

async function testArrayAsyncLimit() {
  async function* gen() {
    yield [new Uint8Array(100)];
    yield [new Uint8Array(100)];
  }
  await assert.rejects(
    () => array(gen(), { limit: 50 }),
    { name: 'RangeError' },
  );
}

// =============================================================================
// Non-array batch tolerance
// =============================================================================

// Regression test: consumers should tolerate sources that yield raw
// Uint8Array or string values instead of Uint8Array[] batches.
async function testConsumersNonArrayBatch() {
  const encoder = new TextEncoder();

  // Source yields raw Uint8Array, not wrapped in an array
  async function* rawSource() {
    yield encoder.encode('hello');
    yield encoder.encode(' world');
  }
  const result = await text(rawSource());
  assert.strictEqual(result, 'hello world');

  // bytes() with raw chunks
  async function* rawSource2() {
    yield encoder.encode('ab');
  }
  const data = await bytes(rawSource2());
  assert.strictEqual(data.length, 2);
  assert.strictEqual(data[0], 97); // 'a'
  assert.strictEqual(data[1], 98); // 'b'

  // array() with raw chunks
  async function* rawSource3() {
    yield encoder.encode('x');
    yield encoder.encode('y');
  }
  const arr = await array(rawSource3());
  assert.strictEqual(arr.length, 2);
}

async function testConsumersNonArrayBatchSync() {
  const encoder = new TextEncoder();

  function* rawSyncSource() {
    yield encoder.encode('sync');
    yield encoder.encode('data');
  }
  const result = textSync(rawSyncSource());
  assert.strictEqual(result, 'syncdata');

  const data = bytesSync(rawSyncSource());
  assert.strictEqual(data.length, 8);

  const arr = arraySync(rawSyncSource());
  assert.strictEqual(arr.length, 2);
}

// Consumers accept string sources directly (normalized via from/fromSync)
async function testBytesStringSource() {
  const result = await bytes('hello-bytes');
  assert.strictEqual(new TextDecoder().decode(result), 'hello-bytes');
}

function testBytesSyncStringSource() {
  const result = bytesSync('hello-sync');
  assert.strictEqual(new TextDecoder().decode(result), 'hello-sync');
}

async function testTextStringSource() {
  const { text } = require('stream/iter');
  const result = await text('direct-string');
  assert.strictEqual(result, 'direct-string');
}

Promise.all([
  testBytesSyncBasic(),
  testBytesSyncLimit(),
  testBytesAsync(),
  testBytesAsyncLimit(),
  testBytesAsyncAbort(),
  testBytesEmpty(),
  testArrayBufferSyncBasic(),
  testArrayBufferAsync(),
  testArraySyncBasic(),
  testArraySyncLimit(),
  testArrayAsync(),
  testArrayAsyncLimit(),
  testConsumersNonArrayBatch(),
  testConsumersNonArrayBatchSync(),
  testBytesStringSource(),
  testBytesSyncStringSource(),
  testTextStringSource(),
]).then(common.mustCall());

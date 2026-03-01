'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from,
  fromSync,
  push,
  bytes,
  bytesSync,
  text,
  textSync,
  arrayBuffer,
  arrayBufferSync,
  array,
  arraySync,
  tap,
  tapSync,
  merge,
} = require('stream/new');

// =============================================================================
// bytesSync / bytes
// =============================================================================

async function testBytesSyncBasic() {
  const source = fromSync('hello');
  const data = bytesSync(source);
  assert.deepStrictEqual(data, new TextEncoder().encode('hello'));
}

async function testBytesSyncLimit() {
  const source = fromSync('hello world');
  assert.throws(
    () => bytesSync(source, { limit: 3 }),
    { name: 'RangeError' },
  );
}

async function testBytesAsync() {
  const source = from('hello-async');
  const data = await bytes(source);
  assert.deepStrictEqual(data, new TextEncoder().encode('hello-async'));
}

async function testBytesAsyncLimit() {
  const source = from('hello world');
  await assert.rejects(
    () => bytes(source, { limit: 3 }),
    { name: 'RangeError' },
  );
}

async function testBytesAsyncAbort() {
  const ac = new AbortController();
  ac.abort();
  const source = from('data');
  await assert.rejects(
    () => bytes(source, { signal: ac.signal }),
    (err) => err.name === 'AbortError',
  );
}

async function testBytesEmpty() {
  const source = from([]);
  const data = await bytes(source);
  assert.strictEqual(data.byteLength, 0);
}

// =============================================================================
// textSync / text
// =============================================================================

async function testTextSyncBasic() {
  const source = fromSync('hello text');
  const data = textSync(source);
  assert.strictEqual(data, 'hello text');
}

async function testTextAsync() {
  const source = from('hello async text');
  const data = await text(source);
  assert.strictEqual(data, 'hello async text');
}

async function testTextEncoding() {
  // Default encoding is utf-8
  const source = from('café');
  const data = await text(source);
  assert.strictEqual(data, 'café');
}

// =============================================================================
// arrayBufferSync / arrayBuffer
// =============================================================================

async function testArrayBufferSyncBasic() {
  const source = fromSync(new Uint8Array([1, 2, 3]));
  const ab = arrayBufferSync(source);
  assert.ok(ab instanceof ArrayBuffer);
  assert.strictEqual(ab.byteLength, 3);
  const view = new Uint8Array(ab);
  assert.deepStrictEqual(view, new Uint8Array([1, 2, 3]));
}

async function testArrayBufferAsync() {
  const source = from(new Uint8Array([10, 20, 30]));
  const ab = await arrayBuffer(source);
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
  const source = fromSync(gen());
  const chunks = arraySync(source);
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
// tap / tapSync
// =============================================================================

async function testTapSync() {
  const observed = [];
  const observer = tapSync((chunks) => {
    if (chunks !== null) {
      observed.push(chunks.length);
    }
  });

  // tapSync returns a function transform
  assert.strictEqual(typeof observer, 'function');

  // Test that it passes data through unchanged
  const input = [new Uint8Array([1]), new Uint8Array([2])];
  const result = observer(input);
  assert.deepStrictEqual(result, input);
  assert.deepStrictEqual(observed, [2]);

  // null (flush) passes through
  const flushResult = observer(null);
  assert.strictEqual(flushResult, null);
}

async function testTapAsync() {
  const observed = [];
  const observer = tap(async (chunks) => {
    if (chunks !== null) {
      observed.push(chunks.length);
    }
  });

  assert.strictEqual(typeof observer, 'function');

  const input = [new Uint8Array([1])];
  const result = await observer(input);
  assert.deepStrictEqual(result, input);
  assert.deepStrictEqual(observed, [1]);
}

async function testTapInPipeline() {
  const { writer, readable } = push();
  const seen = [];

  const observer = tap(async (chunks) => {
    if (chunks !== null) {
      for (const chunk of chunks) {
        seen.push(new TextDecoder().decode(chunk));
      }
    }
  });

  writer.write('hello');
  writer.end();

  // Use pull with tap as a transform
  const { pull } = require('stream/new');
  const result = pull(readable, observer);
  const data = await text(result);

  assert.strictEqual(data, 'hello');
  assert.strictEqual(seen.length, 1);
  assert.strictEqual(seen[0], 'hello');
}

// =============================================================================
// merge
// =============================================================================

async function testMergeTwoSources() {
  const { writer: w1, readable: r1 } = push();
  const { writer: w2, readable: r2 } = push();

  w1.write('from-a');
  w1.end();
  w2.write('from-b');
  w2.end();

  const merged = merge(r1, r2);
  const chunks = [];
  for await (const batch of merged) {
    for (const chunk of batch) {
      chunks.push(new TextDecoder().decode(chunk));
    }
  }

  // Both sources should be present (order is temporal, not guaranteed)
  assert.strictEqual(chunks.length, 2);
  assert.ok(chunks.includes('from-a'));
  assert.ok(chunks.includes('from-b'));
}

async function testMergeSingleSource() {
  const source = from('only-one');
  const merged = merge(source);

  const data = await text(merged);
  assert.strictEqual(data, 'only-one');
}

async function testMergeEmpty() {
  const merged = merge();
  const batches = [];
  for await (const batch of merged) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testMergeWithAbortSignal() {
  const ac = new AbortController();
  ac.abort();

  const source = from('data');
  const merged = merge(source, { signal: ac.signal });

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of merged) {
        assert.fail('Should not reach here');
      }
    },
    (err) => err.name === 'AbortError',
  );
}

Promise.all([
  testBytesSyncBasic(),
  testBytesSyncLimit(),
  testBytesAsync(),
  testBytesAsyncLimit(),
  testBytesAsyncAbort(),
  testBytesEmpty(),
  testTextSyncBasic(),
  testTextAsync(),
  testTextEncoding(),
  testArrayBufferSyncBasic(),
  testArrayBufferAsync(),
  testArraySyncBasic(),
  testArraySyncLimit(),
  testArrayAsync(),
  testArrayAsyncLimit(),
  testTapSync(),
  testTapAsync(),
  testTapInPipeline(),
  testMergeTwoSources(),
  testMergeSingleSource(),
  testMergeEmpty(),
  testMergeWithAbortSignal(),
]).then(common.mustCall());

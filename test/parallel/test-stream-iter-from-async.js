// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { from, text, Stream } = require('stream/iter');

async function testFromString() {
  const readable = from('hello-async');
  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0],
                         new TextEncoder().encode('hello-async'));
}

async function testFromAsyncGenerator() {
  async function* gen() {
    yield new Uint8Array([10, 20]);
    yield new Uint8Array([30, 40]);
  }
  const readable = from(gen());
  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 2);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([10, 20]));
  assert.deepStrictEqual(batches[1][0], new Uint8Array([30, 40]));
}

async function testFromSyncIterableAsAsync() {
  // Sync iterable passed to from() should work
  function* gen() {
    yield new Uint8Array([1]);
    yield new Uint8Array([2]);
  }
  const readable = from(gen());
  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  // Sync iterables get batched together into a single batch
  assert.strictEqual(batches.length, 1);
  assert.strictEqual(batches[0].length, 2);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([1]));
  assert.deepStrictEqual(batches[0][1], new Uint8Array([2]));
}

async function testFromToAsyncStreamableProtocol() {
  const sym = Symbol.for('Stream.toAsyncStreamable');
  const obj = {
    [sym]() {
      return 'async-protocol-data';
    },
  };
  async function* gen() {
    yield obj;
  }
  const readable = from(gen());
  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0],
                         new TextEncoder().encode('async-protocol-data'));
}

function testFromRejectsNonStreamable() {
  assert.throws(
    () => from(12345),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => from(null),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

async function testFromEmptyArray() {
  const readable = from([]);
  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

// Also accessible via Stream namespace
async function testStreamNamespace() {
  const readable = Stream.from('via-namespace');
  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0], new TextEncoder().encode('via-namespace'));
}

async function testCustomToStringInStreamRejects() {
  // Objects with custom toString but no toStreamable protocol are rejected.
  // Use toStreamable protocol instead.
  const obj = { toString() { return 'from toString'; } };
  async function* source() {
    yield obj;
  }
  await assert.rejects(
    () => text(from(source())),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

async function testCustomToPrimitiveInStreamRejects() {
  // Objects with Symbol.toPrimitive but no toStreamable protocol are rejected.
  const obj = {
    [Symbol.toPrimitive](hint) {
      if (hint === 'string') return 'from toPrimitive';
      return 42;
    },
  };
  async function* source() {
    yield obj;
  }
  await assert.rejects(
    () => text(from(source())),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

async function testToStreamableProtocolInStream() {
  // Objects should use toStreamable protocol instead of toString
  const obj = {
    [Symbol.for('Stream.toStreamable')]() { return 'from protocol'; },
  };
  async function* source() {
    yield obj;
  }
  const result = await text(from(source()));
  assert.strictEqual(result, 'from protocol');
}

// Both toAsyncStreamable and toStreamable: async takes precedence
async function testFromAsyncStreamablePrecedence() {
  const obj = {
    [Symbol.for('Stream.toStreamable')]() { return 'sync version'; },
    [Symbol.for('Stream.toAsyncStreamable')]() { return 'async version'; },
  };
  async function* gen() { yield obj; }
  const result = await text(from(gen()));
  assert.strictEqual(result, 'async version');
}

// Top-level toAsyncStreamable protocol on input to from()
async function testFromTopLevelToAsyncStreamable() {
  const obj = {
    [Symbol.for('Stream.toAsyncStreamable')]() {
      return 'top-level-async';
    },
  };
  const result = await text(from(obj));
  assert.strictEqual(result, 'top-level-async');
}

// Top-level toAsyncStreamable returning a Promise
async function testFromTopLevelToAsyncStreamablePromise() {
  const obj = {
    [Symbol.for('Stream.toAsyncStreamable')]() {
      return Promise.resolve('async-promise');
    },
  };
  const result = await text(from(obj));
  assert.strictEqual(result, 'async-promise');
}

// Top-level toStreamable protocol on input to from()
async function testFromTopLevelToStreamable() {
  const obj = {
    [Symbol.for('Stream.toStreamable')]() {
      return 'top-level-sync';
    },
  };
  const result = await text(from(obj));
  assert.strictEqual(result, 'top-level-sync');
}

// Top-level: toAsyncStreamable takes precedence over toStreamable
async function testFromTopLevelAsyncPrecedence() {
  const obj = {
    [Symbol.for('Stream.toStreamable')]() { return 'sync'; },
    [Symbol.for('Stream.toAsyncStreamable')]() { return 'async'; },
  };
  const result = await text(from(obj));
  assert.strictEqual(result, 'async');
}

// Top-level: toAsyncStreamable takes precedence over Symbol.asyncIterator
async function testFromTopLevelProtocolOverIterator() {
  const obj = {
    [Symbol.for('Stream.toAsyncStreamable')]() { return 'from-protocol'; },
    async *[Symbol.asyncIterator]() { yield [new TextEncoder().encode('from-iterator')]; },
  };
  const result = await text(from(obj));
  assert.strictEqual(result, 'from-protocol');
}

// DataView input should be converted to Uint8Array (zero-copy)
async function testFromDataView() {
  const buf = new ArrayBuffer(5);
  const view = new DataView(buf);
  // Write "hello" into the DataView
  view.setUint8(0, 0x68); // h
  view.setUint8(1, 0x65); // e
  view.setUint8(2, 0x6c); // l
  view.setUint8(3, 0x6c); // l
  view.setUint8(4, 0x6f); // o
  const result = await text(from(view));
  assert.strictEqual(result, 'hello');
}

function testFromNullThrows() {
  assert.throws(() => from(null), { code: 'ERR_INVALID_ARG_TYPE' });
}

function testFromUndefinedThrows() {
  assert.throws(() => from(undefined), { code: 'ERR_INVALID_ARG_TYPE' });
}

Promise.all([
  testFromString(),
  testFromAsyncGenerator(),
  testFromSyncIterableAsAsync(),
  testFromToAsyncStreamableProtocol(),
  testFromRejectsNonStreamable(),
  testFromEmptyArray(),
  testStreamNamespace(),
  testCustomToStringInStreamRejects(),
  testCustomToPrimitiveInStreamRejects(),
  testToStreamableProtocolInStream(),
  testFromAsyncStreamablePrecedence(),
  testFromNullThrows(),
  testFromUndefinedThrows(),
  testFromTopLevelToAsyncStreamable(),
  testFromTopLevelToAsyncStreamablePromise(),
  testFromTopLevelToStreamable(),
  testFromTopLevelAsyncPrecedence(),
  testFromTopLevelProtocolOverIterator(),
  testFromDataView(),
]).then(common.mustCall());

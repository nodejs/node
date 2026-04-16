// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { fromSync } = require('stream/iter');

function testFromSyncString() {
  // String input should be UTF-8 encoded
  const readable = fromSync('hello');
  const batches = [];
  for (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.strictEqual(batches[0].length, 1);
  assert.deepStrictEqual(batches[0][0],
                         new TextEncoder().encode('hello'));
}

function testFromSyncUint8Array() {
  const input = new Uint8Array([1, 2, 3]);
  const readable = fromSync(input);
  const batches = [];
  for (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.strictEqual(batches[0].length, 1);
  assert.deepStrictEqual(batches[0][0], input);
}

function testFromSyncArrayBuffer() {
  const ab = new ArrayBuffer(4);
  new Uint8Array(ab).set([10, 20, 30, 40]);
  const readable = fromSync(ab);
  const batches = [];
  for (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([10, 20, 30, 40]));
}

function testFromSyncUint8ArrayArray() {
  // Array of Uint8Array should yield as a single batch
  const chunks = [new Uint8Array([1]), new Uint8Array([2])];
  const readable = fromSync(chunks);
  const batches = [];
  for (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.strictEqual(batches[0].length, 2);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([1]));
  assert.deepStrictEqual(batches[0][1], new Uint8Array([2]));
}

function testFromSyncGenerator() {
  function* gen() {
    yield new Uint8Array([1, 2]);
    yield new Uint8Array([3, 4]);
  }
  const readable = fromSync(gen());
  const batches = [];
  for (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 2);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([1, 2]));
  assert.deepStrictEqual(batches[1][0], new Uint8Array([3, 4]));
}

function testFromSyncNestedIterables() {
  // Nested arrays and strings should be flattened
  function* gen() {
    yield ['hello', ' ', 'world'];
  }
  const readable = fromSync(gen());
  const batches = [];
  for (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.strictEqual(batches[0].length, 3);
  assert.deepStrictEqual(batches[0][0], new TextEncoder().encode('hello'));
  assert.deepStrictEqual(batches[0][1], new TextEncoder().encode(' '));
  assert.deepStrictEqual(batches[0][2], new TextEncoder().encode('world'));
}

function testFromSyncToStreamableProtocol() {
  const sym = Symbol.for('Stream.toStreamable');
  const obj = {
    [sym]() {
      return 'protocol-data';
    },
  };
  function* gen() {
    yield obj;
  }
  const readable = fromSync(gen());
  const batches = [];
  for (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0],
                         new TextEncoder().encode('protocol-data'));
}

function testFromSyncGeneratorError() {
  function* gen() {
    yield new Uint8Array([1]);
    throw new Error('generator boom');
  }
  const readable = fromSync(gen());
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of readable) { /* consume */ }
  }, { message: 'generator boom' });
}

function testFromSyncRejectsNonStreamable() {
  assert.throws(
    () => fromSync(12345),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => fromSync(null),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

function testFromSyncEmptyGenerator() {
  function* empty() {}
  let count = 0;
  // eslint-disable-next-line no-unused-vars
  for (const _ of fromSync(empty())) { count++; }
  assert.strictEqual(count, 0);
}

// Top-level toStreamable protocol on input to fromSync()
function testFromSyncTopLevelToStreamable() {
  const obj = {
    [Symbol.for('Stream.toStreamable')]() {
      return 'top-level-sync';
    },
  };
  const batches = [];
  for (const batch of fromSync(obj)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0],
                         new TextEncoder().encode('top-level-sync'));
}

// Top-level: toStreamable takes precedence over Symbol.iterator
function testFromSyncTopLevelProtocolOverIterator() {
  const obj = {
    [Symbol.for('Stream.toStreamable')]() { return 'from-protocol'; },
    *[Symbol.iterator]() { yield [new TextEncoder().encode('from-iterator')]; },
  };
  const batches = [];
  for (const batch of fromSync(obj)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0],
                         new TextEncoder().encode('from-protocol'));
}

// Top-level: toAsyncStreamable is ignored by fromSync
function testFromSyncIgnoresAsyncStreamable() {
  const obj = {
    [Symbol.for('Stream.toAsyncStreamable')]() { return 'async'; },
  };
  // Has no toStreamable and no Symbol.iterator, should throw
  assert.throws(() => fromSync(obj), { code: 'ERR_INVALID_ARG_TYPE' });
}

// Explicit async iterable rejected
function testFromSyncRejectsAsyncIterable() {
  async function* gen() { yield [new TextEncoder().encode('a')]; }
  assert.throws(() => fromSync(gen()), { code: 'ERR_INVALID_ARG_TYPE' });
}

// Promise rejected
function testFromSyncRejectsPromise() {
  assert.throws(() => fromSync(Promise.resolve('hello')),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// DataView input should be converted to Uint8Array (zero-copy)
function testFromSyncDataView() {
  const buf = new ArrayBuffer(3);
  const view = new DataView(buf);
  view.setUint8(0, 0x48); // H
  view.setUint8(1, 0x49); // I
  view.setUint8(2, 0x21); // !
  const batches = [];
  for (const batch of fromSync(view)) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 1);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([0x48, 0x49, 0x21]));
}

function testFromSyncNullThrows() {
  assert.throws(() => fromSync(null), { code: 'ERR_INVALID_ARG_TYPE' });
}

function testFromSyncUndefinedThrows() {
  assert.throws(() => fromSync(undefined), { code: 'ERR_INVALID_ARG_TYPE' });
}

Promise.all([
  testFromSyncString(),
  testFromSyncUint8Array(),
  testFromSyncArrayBuffer(),
  testFromSyncUint8ArrayArray(),
  testFromSyncGenerator(),
  testFromSyncNestedIterables(),
  testFromSyncToStreamableProtocol(),
  testFromSyncGeneratorError(),
  testFromSyncRejectsNonStreamable(),
  testFromSyncEmptyGenerator(),
  testFromSyncNullThrows(),
  testFromSyncUndefinedThrows(),
  testFromSyncTopLevelToStreamable(),
  testFromSyncTopLevelProtocolOverIterator(),
  testFromSyncIgnoresAsyncStreamable(),
  testFromSyncRejectsAsyncIterable(),
  testFromSyncRejectsPromise(),
  testFromSyncDataView(),
]).then(common.mustCall());

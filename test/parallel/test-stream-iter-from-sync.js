// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { fromSync, textSync } = require('stream/iter');

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
  assert.strictEqual(batches.length, 1);
  assert.strictEqual(batches[0].length, 2);
  assert.deepStrictEqual(batches[0][0], new Uint8Array([1, 2]));
  assert.deepStrictEqual(batches[0][1], new Uint8Array([3, 4]));
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
  assert.throws(() => fromSync(gen()), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /must be a synchronous input, not an async iterable\./,
  });
}

function testFromSyncPrefersIteratorForDualIterable() {
  const input = {
    *[Symbol.iterator]() {
      yield new TextEncoder().encode('sync');
    },
    async *[Symbol.asyncIterator]() {
      yield new TextEncoder().encode('async');
    },
  };

  assert.strictEqual(textSync(fromSync(input)), 'sync');
}

function testFromSyncPrefersIteratorForThenableIterable() {
  const input = {
    then() {},
    *[Symbol.iterator]() {
      yield new TextEncoder().encode('sync');
    },
  };

  assert.strictEqual(textSync(fromSync(input)), 'sync');
}

// Promise rejected
function testFromSyncRejectsPromise() {
  assert.throws(() => fromSync(Promise.resolve('hello')), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /must be a synchronous input, not a promise\./,
  });
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

function testFromSyncFunctionWithToStreamable() {
  // Functions are objects and may implement the protocol.
  function source() {}
  source[Symbol.for('Stream.toStreamable')] = () => 'from-function';
  assert.strictEqual(textSync(fromSync(source)), 'from-function');
  // ...also when nested inside another source.
  assert.strictEqual(textSync(fromSync([source, '!'])), 'from-function!');
  // A function without a protocol is still rejected.
  assert.throws(() => fromSync(() => {}), { code: 'ERR_INVALID_ARG_TYPE' });
}

// The iterators of fromSync() behave as generators do: values are batched
// and normalized, iterables can be iterated again, and return(), throw()
// and errors normalizing a value close the source as for...of does.
function testFromSyncIteratorProtocol() {
  function loggedSource(log, values) {
    return {
      [Symbol.iterator]() {
        let i = 0;
        log.push('iterator');
        return {
          next() {
            log.push(`next ${i}`);
            return i < values.length ?
              { done: false, value: values[i++] } :
              { done: true, value: undefined };
          },
          return() {
            log.push('return');
            return { done: true, value: undefined };
          },
        };
      },
    };
  }
  const lengths = (iterable) => Array.from(iterable, (batch) => batch.length);

  const chunks = Array.from({ length: 130 }, () => new Uint8Array(1));
  let log = [];
  const source = fromSync(loggedSource(log, [
    ...chunks, ['ab', ['cd']], Uint8Array.of(1), [],
  ]));
  assert.deepStrictEqual(lengths(source), [128, 2, 2, 1]);
  assert.strictEqual(log.at(-1), 'next 133');
  assert.deepStrictEqual(lengths(source), [128, 2, 2, 1]);

  // Values that need no normalization.
  for (const value of ['x', Uint8Array.of(1), [Uint8Array.of(1)], []]) {
    const iterable = fromSync(value);
    assert.deepStrictEqual(lengths(iterable), lengths(iterable));
    const iterator = iterable[Symbol.iterator]();
    assert.strictEqual(iterator[Symbol.iterator](), iterator);
    assert.deepStrictEqual({ ...iterator.return('r') },
                           { done: true, value: 'r' });
    assert.strictEqual(iterator.next().done, true);
    assert.throws(() => iterable[Symbol.iterator]().throw(new Error('t')),
                  { message: 't' });
  }
  assert.deepStrictEqual(lengths(fromSync(new Array(300).fill(chunks[0]))),
                         [128, 128, 44]);

  // return() while reading the source, or a value, closes the source;
  // before reading it, or after it ended, does not.
  for (const values of [[[Uint8Array.of(1)], [Uint8Array.of(2)]],
                        [new Array(200).fill('a')]]) {
    log = [];
    const iterator = fromSync(loggedSource(log, values))[Symbol.iterator]();
    iterator.next();
    assert.deepStrictEqual({ ...iterator.return() },
                           { done: true, value: undefined });
    assert.deepStrictEqual(log, ['iterator', 'next 0', 'return']);
    assert.strictEqual(iterator.next().done, true);
  }
  log = [];
  const unstarted = fromSync(loggedSource(log, ['a']))[Symbol.iterator]();
  unstarted.return();
  assert.deepStrictEqual(log, []);

  // throw() and an error normalizing a value close the source, and the
  // error is thrown.
  log = [];
  const thrown = fromSync(loggedSource(log, ['a', 'b']))[Symbol.iterator]();
  thrown.next();
  assert.throws(() => thrown.throw(new Error('thrown')), { message: 'thrown' });
  assert.deepStrictEqual(log, ['iterator', 'next 0', 'return']);
  log = [];
  const invalid = fromSync(loggedSource(log, ['a', 42]))[Symbol.iterator]();
  invalid.next();
  assert.throws(() => invalid.next(), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.deepStrictEqual(log, ['iterator', 'next 0', 'next 1', 'return']);
  assert.strictEqual(invalid.next().done, true);
}

Promise.all([
  testFromSyncIteratorProtocol(),
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
  testFromSyncPrefersIteratorForDualIterable(),
  testFromSyncPrefersIteratorForThenableIterable(),
  testFromSyncRejectsPromise(),
  testFromSyncDataView(),
  testFromSyncFunctionWithToStreamable(),
]).then(common.mustCall());

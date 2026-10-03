// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { bytes, from, text, Stream } = require('stream/iter');
const { setImmediate } = require('timers/promises');

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

async function testFromAsyncIteratorResultShapes() {
  const wrappers = [
    (result) => result,
    (result) => ({
      then(resolve) {
        resolve(result);
      },
    }),
  ];

  for (const wrap of wrappers) {
    let done = false;
    const source = {
      [Symbol.asyncIterator]() {
        return {
          next() {
            if (done) return wrap({ done: true });
            done = true;
            return wrap({ done: false, value: 'data' });
          },
        };
      },
    };

    assert.strictEqual(await text(from(source)), 'data');
  }
}

async function testFromSourceErrorDoesNotWaitForReturn() {
  const reason = new Error('source failed');
  const source = {
    [Symbol.asyncIterator]() {
      return {
        next() {
          return Promise.reject(reason);
        },
        return() {
          return new Promise(() => {});
        },
      };
    },
  };

  await assert.rejects(
    from(source).next(),
    (error) => error === reason,
  );
}

async function testFromBoundsNestedAsyncIterable() {
  let nestedClosed = false;
  async function* nested() {
    try {
      let value = 0;
      while (true) yield new Uint8Array([value++]);
    } finally {
      nestedClosed = true;
    }
  }

  async function* source() {
    yield nested();
  }

  const iterator = from(source())[Symbol.asyncIterator]();
  const first = await iterator.next();
  assert.strictEqual(first.done, false);
  assert.ok(first.value.length > 0);
  assert.ok(first.value.length <= 128);

  await iterator.return();
  assert.strictEqual(nestedClosed, true);
}

async function testFromDoesNotHoldBackNestedAsyncIterable() {
  // Chunks from a nested async iterable must be delivered as they become
  // available, not held back until the nested iterable produces more data
  // or ends.
  const { promise: release, resolve } = Promise.withResolvers();
  async function* nested() {
    yield new Uint8Array([1]);
    await release;
    yield new Uint8Array([2]);
  }

  async function* source() {
    yield nested();
    yield [new Uint8Array([3]), Promise.resolve(new Uint8Array([4]))];
  }

  const iterator = from(source())[Symbol.asyncIterator]();
  assert.deepStrictEqual(await iterator.next(),
                         { done: false, value: [new Uint8Array([1])] });
  resolve();
  const rest = [];
  for (let r = await iterator.next(); !r.done; r = await iterator.next()) {
    for (const chunk of r.value) rest.push(chunk[0]);
  }
  assert.deepStrictEqual(rest, [2, 3, 4]);
}

async function testFromBoundsPreBatchedAsyncValues() {
  // An async source yielding an already-batched Uint8Array[] larger than the
  // batch bound is split, like the same batch from a sync source.
  const big = Array.from({ length: 300 }, (_, i) => new Uint8Array([i & 0xff]));
  async function* source() {
    yield big;
  }
  const sizes = [];
  for await (const batch of from(source())) sizes.push(batch.length);
  assert.deepStrictEqual(sizes, [128, 128, 44]);

  // Batches within the bound are still passed through as-is.
  const small = [new Uint8Array([1]), new Uint8Array([2])];
  async function* smallSource() {
    yield small;
  }
  for await (const batch of from(smallSource())) {
    assert.strictEqual(batch, small);
  }
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

async function testFromSyncIterableAwaitsPromiseValues() {
  const result = await text(from([Promise.resolve('promise-value')]));
  assert.strictEqual(result, 'promise-value');
}

async function testFromSyncIterableRejectsNestedAsyncIterable() {
  async function* asyncGenerator() {
    yield 'data';
  }

  await assert.rejects(
    () => text(from([asyncGenerator()])),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

async function testFromSyncIterableRejectsNestedToAsyncStreamable() {
  const obj = {
    [Symbol.for('Stream.toAsyncStreamable')]() {
      return 'data';
    },
  };

  await assert.rejects(
    () => text(from([obj])),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
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

async function testFromHandlesProtocolRejectionUntilIteration() {
  const reason = new Error('protocol failed');
  const iterable = from({
    [Symbol.for('Stream.toAsyncStreamable')]: common.mustCall(
      () => Promise.reject(reason)),
  });

  await setImmediate();
  await assert.rejects(
    iterable[Symbol.asyncIterator]().next(),
    (error) => error === reason,
  );
}

async function testFromReturnCancelsPendingPromises() {
  const toAsyncStreamable = Symbol.for('Stream.toAsyncStreamable');
  const createSources = [
    (promise) => from([promise]),
    (promise) => from({
      [Symbol.asyncIterator]() {
        let done = false;
        return {
          next() {
            if (done) return { done: true };
            done = true;
            return { done: false, value: promise };
          },
        };
      },
    }),
    (promise) => from({
      [toAsyncStreamable]() {
        return promise;
      },
    }),
  ];

  for (const createSource of createSources) {
    const deferred = Promise.withResolvers();
    const iterator = createSource(deferred.promise)[Symbol.asyncIterator]();
    const read = iterator.next();
    await setImmediate();

    const rejected = assert.rejects(read, { name: 'AbortError' });
    const closed = iterator.return();
    const [, result] = await Promise.all([rejected, closed]);
    assert.strictEqual(result.done, true);
    deferred.resolve('late value');
  }
}

function createPendingNestedSource(
  returnResult = () => ({ done: true })) {
  const started = Promise.withResolvers();
  const pending = Promise.withResolvers();
  let returned = false;
  const nested = {
    [Symbol.asyncIterator]() {
      return {
        next() {
          started.resolve();
          return pending.promise;
        },
        return() {
          returned = true;
          return returnResult();
        },
      };
    },
  };

  async function* source() {
    yield nested;
  }

  return {
    source: source(),
    started: started.promise,
    resolve: pending.resolve,
    wasReturned() {
      return returned;
    },
  };
}

async function testFromReturnClosesPendingNestedIterator() {
  const fixture = createPendingNestedSource();
  const iterator = from(fixture.source)[Symbol.asyncIterator]();
  const read = iterator.next();
  await fixture.started;

  const rejected = assert.rejects(read, { name: 'AbortError' });
  const closed = iterator.return();
  await Promise.all([rejected, closed]);
  assert.strictEqual(fixture.wasReturned(), true);
  fixture.resolve({ done: true });
}

async function testConsumerAbortClosesPendingNestedIterator() {
  const fixture = createPendingNestedSource();
  const controller = new AbortController();
  const reason = new Error('consumer cancelled');
  const consumed = bytes(fixture.source, { signal: controller.signal });
  await fixture.started;

  const rejected = assert.rejects(consumed, (error) => error === reason);
  controller.abort(reason);
  await rejected;
  await setImmediate();
  assert.strictEqual(fixture.wasReturned(), true);
  fixture.resolve({ done: true });
}

async function testFromCancellationHandlesCleanupRejection() {
  const fixture = createPendingNestedSource(
    () => Promise.reject(new Error('cleanup failed')));
  const iterator = from(fixture.source)[Symbol.asyncIterator]();
  const read = iterator.next();
  await fixture.started;

  const rejected = assert.rejects(read, { name: 'AbortError' });
  await Promise.all([rejected, iterator.return()]);
  await setImmediate();
  assert.strictEqual(fixture.wasReturned(), true);
  fixture.resolve({ done: true });
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

async function testFromFunctionWithProtocols() {
  // Functions are objects and may implement the protocols.
  function asyncSource() {}
  asyncSource[Symbol.for('Stream.toAsyncStreamable')] =
    async () => 'async-function';
  assert.strictEqual(await text(from(asyncSource)), 'async-function');

  function syncSource() {}
  syncSource[Symbol.for('Stream.toStreamable')] = () => 'sync-function';
  assert.strictEqual(await text(from(syncSource)), 'sync-function');

  async function* nested() {
    yield asyncSource;
    yield syncSource;
  }
  assert.strictEqual(await text(from(nested())),
                     'async-functionsync-function');

  // A function without a protocol is still rejected.
  assert.throws(() => from(() => {}), { code: 'ERR_INVALID_ARG_TYPE' });
}

Promise.all([
  testFromString(),
  testFromAsyncGenerator(),
  testFromAsyncIteratorResultShapes(),
  testFromSourceErrorDoesNotWaitForReturn(),
  testFromBoundsNestedAsyncIterable(),
  testFromFunctionWithProtocols(),
  testFromDoesNotHoldBackNestedAsyncIterable(),
  testFromBoundsPreBatchedAsyncValues(),
  testFromSyncIterableAsAsync(),
  testFromSyncIterableAwaitsPromiseValues(),
  testFromSyncIterableRejectsNestedAsyncIterable(),
  testFromSyncIterableRejectsNestedToAsyncStreamable(),
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
  testFromHandlesProtocolRejectionUntilIteration(),
  testFromReturnCancelsPendingPromises(),
  testFromReturnClosesPendingNestedIterator(),
  testConsumerAbortClosesPendingNestedIterator(),
  testFromCancellationHandlesCleanupRejection(),
  testFromDataView(),
]).then(common.mustCall());

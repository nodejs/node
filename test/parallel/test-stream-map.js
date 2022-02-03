'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');
const { setTimeout } = require('timers/promises');

{
  // Map works on synchronous streams with a synchronous mapper
  const stream = Readable.from([1, 2, 3, 4, 5]).map((x) => x + x);
  const result = [2, 4, 6, 8, 10];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // Map works on synchronous streams with an asynchronous mapper
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    await Promise.resolve();
    return x + x;
  });
  const result = [2, 4, 6, 8, 10];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // Map works on asynchronous streams with a asynchronous mapper
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    return x + x;
  }).map((x) => x + x);
  const result = [4, 8, 12, 16, 20];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // Concurrency + AbortSignal
  const ac = new AbortController();
  let calls = 0;
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (_, { signal }) => {
    calls++;
    await setTimeout(100, { signal });
  }, { signal: ac.signal, concurrency: 2 });
  // pump
  assert.rejects(async () => {
    for await (const item of stream) {
      // nope
      console.log(item);
    }
  }, {
    name: 'AbortError',
  }).then(common.mustCall());

  setImmediate(() => {
    ac.abort();
    assert.strictEqual(calls, 2);
  });
}

{
  // Concurrency result order
  const stream = Readable.from([1, 2]).map(async (item, { signal }) => {
    await setTimeout(10 - item, { signal });
    return item;
  }, { concurrency: 2 });

  (async () => {
    const expected = [1, 2];
    for await (const item of stream) {
      assert.strictEqual(item, expected.shift());
    }
  })().then(common.mustCall());
}

{
  // Error cases
  assert.throws(() => Readable.from([1]).map(1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => Readable.from([1]).map((x) => x, {
    concurrency: 'Foo'
  }), /ERR_OUT_OF_RANGE/);
  assert.throws(() => Readable.from([1]).map((x) => x, 1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => Readable.from([1]).map((x) => x, { signal: true }), /ERR_INVALID_ARG_TYPE/);
}
{
  // Test result is a Readable
  const stream = Readable.from([1, 2, 3, 4, 5]).map((x) => x);
  assert.strictEqual(stream.readable, true);
}

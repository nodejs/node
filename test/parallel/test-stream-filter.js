'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');
const { setTimeout } = require('timers/promises');

{
  // Filter works on synchronous streams with a synchronous predicate
  const stream = Readable.from([1, 2, 3, 4, 5]).filter((x) => x < 3);
  const result = [1, 2];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // Filter works on synchronous streams with an asynchronous predicate
  const stream = Readable.from([1, 2, 3, 4, 5]).filter(async (x) => {
    await Promise.resolve();
    return x > 3;
  });
  const result = [4, 5];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // Map works on asynchronous streams with a asynchronous mapper
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    await Promise.resolve();
    return x + x;
  }).filter((x) => x > 5);
  const result = [6, 8, 10];
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
  const stream = Readable.from([1, 2, 3, 4]).filter(async (_, { signal }) => {
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
  const stream = Readable.from([1, 2]).filter(async (item, { signal }) => {
    await setTimeout(10 - item, { signal });
    return true;
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
  assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const unused of Readable.from([1]).filter(1));
  }, /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
  assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of Readable.from([1]).filter((x) => x, {
      concurrency: 'Foo'
    }));
  }, /ERR_OUT_OF_RANGE/).then(common.mustCall());
  assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of Readable.from([1]).filter((x) => x, 1));
  }, /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
}
{
  // Test result is a Readable
  const stream = Readable.from([1, 2, 3, 4, 5]).filter((x) => true);
  assert.strictEqual(stream.readable, true);
}

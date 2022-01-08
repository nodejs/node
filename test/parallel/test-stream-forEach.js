'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');
const { setTimeout } = require('timers/promises');

{
  // forEach works on synchronous streams with a synchronous predicate
  const stream = Readable.from([1, 2, 3]);
  const result = [1, 2, 3];
  (async () => {
    await stream.forEach((value) => assert.strictEqual(value, result.shift()));
  })().then(common.mustCall());
}

{
  // forEach works an asynchronous streams
  const stream = Readable.from([1, 2, 3]).filter(async (x) => {
    await Promise.resolve();
    return true;
  });
  const result = [1, 2, 3];
  (async () => {
    await stream.forEach((value) => assert.strictEqual(value, result.shift()));
  })().then(common.mustCall());
}

{
  // forEach works on asynchronous streams with a asynchronous forEach fn
  const stream = Readable.from([1, 2, 3]).filter(async (x) => {
    await Promise.resolve();
    return true;
  });
  const result = [1, 2, 3];
  (async () => {
    await stream.forEach(async (value) => {
      await Promise.resolve();
      assert.strictEqual(value, result.shift());
    });
  })().then(common.mustCall());
}

{
  // Concurrency + AbortSignal
  const ac = new AbortController();
  let calls = 0;
  const forEachPromise =
    Readable.from([1, 2, 3, 4]).forEach(async (_, { signal }) => {
      calls++;
      await setTimeout(100, { signal });
    }, { signal: ac.signal, concurrency: 2 });
  // pump
  assert.rejects(async () => {
    await forEachPromise;
  }, {
    name: 'AbortError',
  }).then(common.mustCall());

  setImmediate(() => {
    ac.abort();
    assert.strictEqual(calls, 2);
  });
}

{
  // Error cases
  assert.rejects(async () => {
    await Readable.from([1]).forEach(1);
  }, /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
  assert.rejects(async () => {
    await Readable.from([1]).forEach((x) => x, {
      concurrency: 'Foo'
    });
  }, /ERR_OUT_OF_RANGE/).then(common.mustCall());
  assert.rejects(async () => {
    await Readable.from([1]).forEach((x) => x, 1);
  }, /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
}
{
  // Test result is a Promise
  const stream = Readable.from([1, 2, 3, 4, 5]).forEach((_) => true);
  assert.strictEqual(typeof stream.then, 'function');
}

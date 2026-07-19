'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');

const { from } = Readable;

const fromAsync = (...args) => from(...args).map(async (x) => x);

const naturals = () => from(async function*() {
  let i = 1;
  while (true) {
    yield i++;
  }
}());

{
  // Synchronous streams
  (async () => {
    assert.deepStrictEqual(await from([1, 2, 3]).drop(2).toArray(), [3]);
    assert.deepStrictEqual(await from([1, 2, 3]).take(1).toArray(), [1]);
    assert.deepStrictEqual(await from([]).drop(2).toArray(), []);
    assert.deepStrictEqual(await from([]).take(1).toArray(), []);
    assert.deepStrictEqual(await from([1, 2, 3]).drop(1).take(1).toArray(), [2]);
    assert.deepStrictEqual(await from([1, 2]).drop(0).toArray(), [1, 2]);
    assert.deepStrictEqual(await from([1, 2]).take(0).toArray(), []);
  })().then(common.mustCall());
  // Asynchronous streams
  (async () => {
    assert.deepStrictEqual(await fromAsync([1, 2, 3]).drop(2).toArray(), [3]);
    assert.deepStrictEqual(await fromAsync([1, 2, 3]).take(1).toArray(), [1]);
    assert.deepStrictEqual(await fromAsync([]).drop(2).toArray(), []);
    assert.deepStrictEqual(await fromAsync([]).take(1).toArray(), []);
    assert.deepStrictEqual(await fromAsync([1, 2, 3]).drop(1).take(1).toArray(), [2]);
    assert.deepStrictEqual(await fromAsync([1, 2]).drop(0).toArray(), [1, 2]);
    assert.deepStrictEqual(await fromAsync([1, 2]).take(0).toArray(), []);
  })().then(common.mustCall());
  // Infinite streams
  // Asynchronous streams
  (async () => {
    assert.deepStrictEqual(await naturals().take(1).toArray(), [1]);
    assert.deepStrictEqual(await naturals().drop(1).take(1).toArray(), [2]);
    const next10 = [11, 12, 13, 14, 15, 16, 17, 18, 19, 20];
    assert.deepStrictEqual(await naturals().drop(10).take(10).toArray(), next10);
    assert.deepStrictEqual(await naturals().take(5).take(1).toArray(), [1]);
  })().then(common.mustCall());
}


// Don't wait for next item in the original stream when already consumed the requested take amount
{
  let reached = false;
  let resolve;
  const promise = new Promise((res) => resolve = res);

  const stream = from((async function *() {
    yield 1;
    await promise;
    reached = true;
    yield 2;
  })());

  stream.take(1)
    .toArray()
    .then(common.mustCall(() => {
      assert.strictEqual(reached, false);
    }))
    .finally(() => resolve());
}

{
  // Coercion
  (async () => {
    // The spec made me do this ^^
    assert.deepStrictEqual(await naturals().take('cat').toArray(), []);
    assert.deepStrictEqual(await naturals().take('2').toArray(), [1, 2]);
    assert.deepStrictEqual(await naturals().take(true).toArray(), [1]);
  })().then(common.mustCall());
}

{
  // Support for AbortSignal
  const ac = new AbortController();
  assert.rejects(
    Readable.from([1, 2, 3]).take(1, { signal: ac.signal }).toArray(), {
      name: 'AbortError',
    }).then(common.mustCall());
  assert.rejects(
    Readable.from([1, 2, 3]).drop(1, { signal: ac.signal }).toArray(), {
      name: 'AbortError',
    }).then(common.mustCall());
  ac.abort();
}

{
  // Support for AbortSignal, already aborted
  const signal = AbortSignal.abort();
  assert.rejects(
    Readable.from([1, 2, 3]).take(1, { signal }).toArray(), {
      name: 'AbortError',
    }).then(common.mustCall());
}

{
  // Error cases
  const invalidArgs = [
    -1,
    -Infinity,
    -40,
  ];

  for (const example of invalidArgs) {
    assert.throws(() => from([]).take(example).toArray(), /ERR_OUT_OF_RANGE/);
  }

  assert.throws(() => Readable.from([1]).drop(1, 1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => Readable.from([1]).drop(1, { signal: true }), /ERR_INVALID_ARG_TYPE/);

  assert.throws(() => Readable.from([1]).take(1, 1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => Readable.from([1]).take(1, { signal: true }), /ERR_INVALID_ARG_TYPE/);
}

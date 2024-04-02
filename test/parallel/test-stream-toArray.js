'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');

{
  // Works on a synchronous stream
  (async () => {
    const tests = [
      [],
      [1],
      [1, 2, 3],
      Array(100).fill().map((_, i) => i),
    ];
    for (const test of tests) {
      const stream = Readable.from(test);
      const result = await stream.toArray();
      assert.deepStrictEqual(result, test);
    }
  })().then(common.mustCall());
}

{
  // Works on a non-object-mode stream
  (async () => {
    const firstBuffer = Buffer.from([1, 2, 3]);
    const secondBuffer = Buffer.from([4, 5, 6]);
    const stream = Readable.from(
      [firstBuffer, secondBuffer],
      { objectMode: false });
    const result = await stream.toArray();
    assert.strictEqual(Array.isArray(result), true);
    assert.deepStrictEqual(result, [firstBuffer, secondBuffer]);
  })().then(common.mustCall());
}

{
  // Works on an asynchronous stream
  (async () => {
    const tests = [
      [],
      [1],
      [1, 2, 3],
      Array(100).fill().map((_, i) => i),
    ];
    for (const test of tests) {
      const stream = Readable.from(test).map((x) => Promise.resolve(x));
      const result = await stream.toArray();
      assert.deepStrictEqual(result, test);
    }
  })().then(common.mustCall());
}

{
  // Support for AbortSignal
  const ac = new AbortController();
  let stream;
  assert.rejects(async () => {
    stream = Readable.from([1, 2, 3]).map(async (x) => {
      if (x === 3) {
        await new Promise(() => {}); // Explicitly do not pass signal here
      }
      return Promise.resolve(x);
    });
    await stream.toArray({ signal: ac.signal });
  }, {
    name: 'AbortError',
  }).then(common.mustCall(() => {
    // Only stops toArray, does not destroy the stream
    assert(stream.destroyed, false);
  }));
  ac.abort();
}
{
  // Test result is a Promise
  const result = Readable.from([1, 2, 3, 4, 5]).toArray();
  assert.strictEqual(result instanceof Promise, true);
}
{
  // Error cases
  assert.rejects(async () => {
    await Readable.from([1]).toArray(1);
  }, /ERR_INVALID_ARG_TYPE/).then(common.mustCall());

  assert.rejects(async () => {
    await Readable.from([1]).toArray({
      signal: true
    });
  }, /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
}

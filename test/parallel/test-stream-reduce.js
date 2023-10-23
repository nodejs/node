'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');

function sum(p, c) {
  return p + c;
}

{
  // Does the same thing as `(await stream.toArray()).reduce(...)`
  (async () => {
    const tests = [
      [[], sum, 0],
      [[1], sum, 0],
      [[1, 2, 3, 4, 5], sum, 0],
      [[...Array(100).keys()], sum, 0],
      [['a', 'b', 'c'], sum, ''],
      [[1, 2], sum],
      [[1, 2, 3], (x, y) => y],
    ];
    for (const [values, fn, initial] of tests) {
      const streamReduce = await Readable.from(values)
                                         .reduce(fn, initial);
      const arrayReduce = values.reduce(fn, initial);
      assert.deepStrictEqual(streamReduce, arrayReduce);
    }
    // Does the same thing as `(await stream.toArray()).reduce(...)` with an
    // asynchronous reducer
    for (const [values, fn, initial] of tests) {
      const streamReduce = await Readable.from(values)
                                         .map(async (x) => x)
                                         .reduce(fn, initial);
      const arrayReduce = values.reduce(fn, initial);
      assert.deepStrictEqual(streamReduce, arrayReduce);
    }
  })().then(common.mustCall());
}
{
  // Works with an async reducer, with or without initial value
  (async () => {
    const six = await Readable.from([1, 2, 3]).reduce(async (p, c) => p + c, 0);
    assert.strictEqual(six, 6);
  })().then(common.mustCall());
  (async () => {
    const six = await Readable.from([1, 2, 3]).reduce(async (p, c) => p + c);
    assert.strictEqual(six, 6);
  })().then(common.mustCall());
}
{
  // Works lazily
  assert.rejects(Readable.from([1, 2, 3, 4, 5, 6])
    .map(common.mustCall((x) => {
      return x;
    }, 3)) // Two consumed and one buffered by `map` due to default concurrency
    .reduce(async (p, c) => {
      if (p === 1) {
        throw new Error('boom');
      }
      return c;
    }, 0)
  , /boom/).then(common.mustCall());
}

{
  // Support for AbortSignal
  const ac = new AbortController();
  assert.rejects(async () => {
    await Readable.from([1, 2, 3]).reduce(async (p, c) => {
      if (c === 3) {
        await new Promise(() => {}); // Explicitly do not pass signal here
      }
      return Promise.resolve();
    }, 0, { signal: ac.signal });
  }, {
    name: 'AbortError',
  }).then(common.mustCall());
  ac.abort();
}


{
  // Support for AbortSignal - pre aborted
  const stream = Readable.from([1, 2, 3]);
  assert.rejects(async () => {
    await stream.reduce(async (p, c) => {
      if (c === 3) {
        await new Promise(() => {}); // Explicitly do not pass signal here
      }
      return Promise.resolve();
    }, 0, { signal: AbortSignal.abort() });
  }, {
    name: 'AbortError',
  }).then(common.mustCall(() => {
    assert.strictEqual(stream.destroyed, true);
  }));
}

{
  // Support for AbortSignal - deep
  const stream = Readable.from([1, 2, 3]);
  assert.rejects(async () => {
    await stream.reduce(async (p, c, { signal }) => {
      signal.addEventListener('abort', common.mustCall(), { once: true });
      if (c === 3) {
        await new Promise(() => {}); // Explicitly do not pass signal here
      }
      return Promise.resolve();
    }, 0, { signal: AbortSignal.abort() });
  }, {
    name: 'AbortError',
  }).then(common.mustCall(() => {
    assert.strictEqual(stream.destroyed, true);
  }));
}

{
  // Error cases
  assert.rejects(() => Readable.from([]).reduce(1), /TypeError/).then(common.mustCall());
  assert.rejects(() => Readable.from([]).reduce('5'), /TypeError/).then(common.mustCall());
  assert.rejects(() => Readable.from([]).reduce((x, y) => x + y, 0, 1), /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
  assert.rejects(() => Readable.from([]).reduce((x, y) => x + y, 0, { signal: true }), /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
}

{
  // Test result is a Promise
  const result = Readable.from([1, 2, 3, 4, 5]).reduce(sum, 0);
  assert.ok(result instanceof Promise);
}

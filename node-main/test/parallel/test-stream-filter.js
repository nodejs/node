'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');
const { once } = require('events');
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
  // Filter works on an infinite stream
  const stream = Readable.from(async function* () {
    while (true) yield 1;
  }()).filter(common.mustCall(async (x) => {
    return x < 3;
  }, 5));
  (async () => {
    let i = 1;
    for await (const item of stream) {
      assert.strictEqual(item, 1);
      if (++i === 5) break;
    }
  })().then(common.mustCall());
}

{
  // Filter works on constructor created streams
  let i = 0;
  const stream = new Readable({
    read() {
      if (i === 10) {
        this.push(null);
        return;
      }
      this.push(Uint8Array.from([i]));
      i++;
    },
    highWaterMark: 0,
  }).filter(common.mustCall(async ([x]) => {
    return x !== 5;
  }, 10));
  (async () => {
    const result = (await stream.toArray()).map((x) => x[0]);
    const expected = [...Array(10).keys()].filter((x) => x !== 5);
    assert.deepStrictEqual(result, expected);
  })().then(common.mustCall());
}

{
  // Throwing an error during `filter` (sync)
  const stream = Readable.from([1, 2, 3, 4, 5]).filter((x) => {
    if (x === 3) {
      throw new Error('boom');
    }
    return true;
  });
  assert.rejects(
    stream.map((x) => x + x).toArray(),
    /boom/,
  ).then(common.mustCall());
}

{
  // Throwing an error during `filter` (async)
  const stream = Readable.from([1, 2, 3, 4, 5]).filter(async (x) => {
    if (x === 3) {
      throw new Error('boom');
    }
    return true;
  });
  assert.rejects(
    stream.filter(() => true).toArray(),
    /boom/,
  ).then(common.mustCall());
}

{
  // Concurrency + AbortSignal
  const ac = new AbortController();
  let calls = 0;
  const stream = Readable.from([1, 2, 3, 4]).filter(async (_, { signal }) => {
    calls++;
    await once(signal, 'abort');
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
  assert.throws(() => Readable.from([1]).filter(1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => Readable.from([1]).filter((x) => x, {
    concurrency: 'Foo'
  }), /ERR_OUT_OF_RANGE/);
  assert.throws(() => Readable.from([1]).filter((x) => x, 1), /ERR_INVALID_ARG_TYPE/);
}
{
  // Test result is a Readable
  const stream = Readable.from([1, 2, 3, 4, 5]).filter((x) => true);
  assert.strictEqual(stream.readable, true);
}
{
  const stream = Readable.from([1, 2, 3, 4, 5]);
  Object.defineProperty(stream, 'map', {
    value: common.mustNotCall(),
  });
  // Check that map isn't getting called.
  stream.filter(() => true);
}

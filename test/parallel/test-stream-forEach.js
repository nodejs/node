'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');
const { once } = require('events');

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
  // forEach works on an infinite stream
  const ac = new AbortController();
  const { signal } = ac;
  const stream = Readable.from(async function* () {
    while (true) yield 1;
  }(), { signal });
  let i = 0;
  assert.rejects(stream.forEach(common.mustCall((x) => {
    i++;
    if (i === 10) ac.abort();
    assert.strictEqual(x, 1);
  }, 10)), { name: 'AbortError' }).then(common.mustCall());
}

{
  // Emitting an error during `forEach`
  const stream = Readable.from([1, 2, 3, 4, 5]);
  assert.rejects(stream.forEach(async (x) => {
    if (x === 3) {
      stream.emit('error', new Error('boom'));
    }
  }), /boom/).then(common.mustCall());
}

{
  // Throwing an error during `forEach` (sync)
  const stream = Readable.from([1, 2, 3, 4, 5]);
  assert.rejects(stream.forEach((x) => {
    if (x === 3) {
      throw new Error('boom');
    }
  }), /boom/).then(common.mustCall());
}

{
  // Throwing an error during `forEach` (async)
  const stream = Readable.from([1, 2, 3, 4, 5]);
  assert.rejects(stream.forEach(async (x) => {
    if (x === 3) {
      return Promise.reject(new Error('boom'));
    }
  }), /boom/).then(common.mustCall());
}

{
  // Concurrency + AbortSignal
  const ac = new AbortController();
  let calls = 0;
  const forEachPromise =
    Readable.from([1, 2, 3, 4]).forEach(async (_, { signal }) => {
      calls++;
      await once(signal, 'abort');
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
{
  const stream = Readable.from([1, 2, 3, 4, 5]);
  Object.defineProperty(stream, 'map', {
    value: common.mustNotCall(() => {}),
  });
  // Check that map isn't getting called.
  stream.forEach(() => true);
}

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
  // destroyOnReturn can preserve streams that do not auto-destroy.
  const stream = new Readable({
    objectMode: true,
    autoDestroy: false,
    read() {
      this.push(1);
      this.push(2);
      this.push(null);
    },
  });

  (async () => {
    assert.deepStrictEqual(
      await stream.toArray({ destroyOnReturn: false }),
      [1, 2],
    );
    assert.strictEqual(stream.destroyed, false);
    for (const event of ['end', 'finish', 'error', 'close']) {
      assert.strictEqual(stream.listenerCount(event), 0);
    }
    stream.destroy();
  })().then(common.mustCall());
}

{
  // Support for AbortSignal
  const ac = new AbortController();
  let stream;
  assert.rejects(async () => {
    stream = Readable.from([1, 2, 3, 4]).map(async (x) => {
      if (x === 3) {
        await new Promise(() => {}); // Explicitly do not pass signal here
      }
      return Promise.resolve(x);
    });
    await stream.toArray({ signal: ac.signal });
  }, {
    name: 'AbortError',
  }).then(common.mustCall(() => {
    // Stops toArray *and* destroys the stream
    assert.strictEqual(stream.destroyed, true);
  }));
  ac.abort();
}

{
  // AbortSignal wakes toArray while its source is idle.
  const ac = new AbortController();
  const stream = new Readable({ read() {} });
  const result = stream.toArray({ signal: ac.signal });

  setImmediate(() => ac.abort());
  assert.rejects(result, { name: 'AbortError' }).then(common.mustCall(() => {
    assert.strictEqual(stream.listenerCount('readable'), 0);
  }));
}

{
  // A pre-aborted signal prevents the source from being read.
  const stream = new Readable({
    read: common.mustNotCall(),
  });

  assert.rejects(
    stream.toArray({ signal: AbortSignal.abort() }),
    { name: 'AbortError' },
  ).then(common.mustCall());
}

{
  // Source errors reject and remove the readable listener.
  const error = new Error('boom');
  const stream = new Readable({
    read() {
      this.destroy(error);
    },
  });

  assert.rejects(stream.toArray(), error).then(common.mustCall(() => {
    assert.strictEqual(stream.listenerCount('readable'), 0);
  }));
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

  assert.rejects(async () => {
    await Readable.from([1]).toArray({
      destroyOnReturn: 'false'
    });
  }, /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
}

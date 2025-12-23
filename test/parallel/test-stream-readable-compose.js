'use strict';

const common = require('../common');
const {
  PassThrough,
  Readable,
  Transform,
} = require('stream');
const assert = require('assert');

{
  // with async generator
  const stream = Readable.from(['a', 'b', 'c', 'd']).compose(async function *(stream) {
    let str = '';
    for await (const chunk of stream) {
      str += chunk;

      if (str.length === 2) {
        yield str;
        str = '';
      }
    }
  });
  assert.strictEqual(stream.readable, true);
  assert.strictEqual(stream.writable, false);
  const result = ['ab', 'cd'];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // With Transformer
  const stream = Readable.from(['a', 'b', 'c', 'd']).compose(new Transform({
    objectMode: true,
    transform: common.mustCall((chunk, encoding, callback) => {
      callback(null, chunk);
    }, 4)
  }));
  assert.strictEqual(stream.readable, true);
  assert.strictEqual(stream.writable, false);
  const result = ['a', 'b', 'c', 'd'];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // With Duplex stream as `this`, ensuring writes to the composed stream
  // are passed to the head of the pipeline
  const pt = new PassThrough({ objectMode: true });
  const composed = pt.compose(async function *(stream) {
    for await (const chunk of stream) {
      yield chunk * 2;
    }
  });
  assert.strictEqual(composed.readable, true);
  assert.strictEqual(composed.writable, true);
  pt.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk, 123);
  }));
  composed.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk, 246);
  }));
  composed.end(123);
}

{
  // Throwing an error during `compose` (before waiting for data)
  const stream = Readable.from([1, 2, 3, 4, 5]).compose(async function *(stream) { // eslint-disable-line require-yield

    throw new Error('boom');
  });

  assert.rejects(async () => {
    for await (const item of stream) {
      assert.fail('should not reach here, got ' + item);
    }
  }, /boom/).then(common.mustCall());
}

{
  // Throwing an error during `compose` (when waiting for data)
  const stream = Readable.from([1, 2, 3, 4, 5]).compose(async function *(stream) {
    for await (const chunk of stream) {
      if (chunk === 3) {
        throw new Error('boom');
      }
      yield chunk;
    }
  });

  assert.rejects(
    stream.toArray(),
    /boom/,
  ).then(common.mustCall());
}

{
  // Throwing an error during `compose` (after finishing all readable data)
  const stream = Readable.from([1, 2, 3, 4, 5]).compose(async function *(stream) { // eslint-disable-line require-yield

    // eslint-disable-next-line no-unused-vars,no-empty
    for await (const chunk of stream) {
    }

    throw new Error('boom');
  });
  assert.rejects(
    stream.toArray(),
    /boom/,
  ).then(common.mustCall());
}

{
  // AbortSignal
  const ac = new AbortController();
  const stream = Readable.from([1, 2, 3, 4, 5])
    .compose(async function *(source) {
      // Should not reach here
      for await (const chunk of source) {
        yield chunk;
      }
    }, { signal: ac.signal });

  ac.abort();

  assert.rejects(async () => {
    for await (const item of stream) {
      assert.fail('should not reach here, got ' + item);
    }
  }, {
    name: 'AbortError',
  }).then(common.mustCall());
}

{
  assert.throws(
    () => Readable.from(['a']).compose(Readable.from(['b'])),
    { code: 'ERR_INVALID_ARG_VALUE' }
  );
}

{
  assert.throws(
    () => Readable.from(['a']).compose(),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
}

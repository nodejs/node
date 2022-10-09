'use strict';

const common = require('../common');
const {
  Readable, Transform,
} = require('stream');
const assert = require('assert');
const { once } = require('events');

{
  // compose operator with async generator
  const stream = Readable.from(['a', 'b', 'c', 'd']).compose(async function *(stream) {
    let str = '';
    for await (const chunk of stream) {
      str += chunk;

      if(str.length === 2) {
        yield str;
        str = '';
      }
    }
  });
  const result = ['ab', 'cd'];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // compose operator with Transformer
  const stream = Readable.from(['a', 'b', 'c', 'd']).compose(new Transform({
    objectMode: true,
    transform: common.mustCall((chunk, encoding, callback) => {
      callback(null, chunk);
    })
  }));
  const result = ['a', 'b', 'c', 'd'];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // Throwing an error during `compose` (before waiting for data)
  const stream = Readable.from([1, 2, 3, 4, 5]).compose(async function *(stream) {
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
  const stream = Readable.from([1, 2, 3, 4, 5]).compose(async function *(stream) {
    for await (const chunk of stream) {
    }

    throw new Error('boom');
  });
  assert.rejects(
    stream.toArray(),
    /boom/,
  ).then(common.mustCall());
}

// TODO - add here tests for abort signal and argument validation

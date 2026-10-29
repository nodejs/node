'use strict';

const { mustCall } = require('../common');
const { once } = require('events');
const { Readable } = require('stream');
const assert = require('assert');
const common = require('../common');

{
  assert.throws(() => {
    Readable.from(null);
  }, /ERR_INVALID_ARG_TYPE/);
}

async function toReadableBasicSupport() {
  async function* generate() {
    yield 'a';
    yield 'b';
    yield 'c';
  }

  const stream = Readable.from(generate());

  const expected = ['a', 'b', 'c'];

  for await (const chunk of stream) {
    assert.strictEqual(chunk, expected.shift());
  }
}

async function toReadableSyncIterator() {
  function* generate() {
    yield 'a';
    yield 'b';
    yield 'c';
  }

  const stream = Readable.from(generate());

  const expected = ['a', 'b', 'c'];

  for await (const chunk of stream) {
    assert.strictEqual(chunk, expected.shift());
  }
}

async function toReadablePromises() {
  const promises = [
    Promise.resolve('a'),
    Promise.resolve('b'),
    Promise.resolve('c'),
  ];

  const stream = Readable.from(promises);

  const expected = ['a', 'b', 'c'];

  for await (const chunk of stream) {
    assert.strictEqual(chunk, expected.shift());
  }
}

async function toReadableString() {
  const stream = Readable.from('abc');

  const expected = ['abc'];

  for await (const chunk of stream) {
    assert.strictEqual(chunk, expected.shift());
  }
}

async function toReadableBuffer() {
  const stream = Readable.from(Buffer.from('abc'));

  const expected = ['abc'];

  for await (const chunk of stream) {
    assert.strictEqual(chunk.toString(), expected.shift());
  }
}

async function toReadableOnData() {
  async function* generate() {
    yield 'a';
    yield 'b';
    yield 'c';
  }

  const stream = Readable.from(generate());

  let iterations = 0;
  const expected = ['a', 'b', 'c'];

  stream.on('data', common.mustCallAtLeast((chunk) => {
    iterations++;
    assert.strictEqual(chunk, expected.shift());
  }));

  await once(stream, 'end');

  assert.strictEqual(iterations, 3);
}

async function toReadableOnDataNonObject() {
  async function* generate() {
    yield 'a';
    yield 'b';
    yield 'c';
  }

  const stream = Readable.from(generate(), { objectMode: false });

  let iterations = 0;
  const expected = ['a', 'b', 'c'];

  stream.on('data', common.mustCallAtLeast((chunk) => {
    iterations++;
    assert.strictEqual(chunk instanceof Buffer, true);
    assert.strictEqual(chunk.toString(), expected.shift());
  }));

  await once(stream, 'end');

  assert.strictEqual(iterations, 3);
}

async function destroysTheStreamWhenThrowing() {
  async function* generate() { // eslint-disable-line require-yield
    throw new Error('kaboom');
  }

  const stream = Readable.from(generate());

  stream.read();

  const [err] = await once(stream, 'error');
  assert.strictEqual(err.message, 'kaboom');
  assert.strictEqual(stream.destroyed, true);

}

async function asTransformStream() {
  async function* generate(stream) {
    for await (const chunk of stream) {
      yield chunk.toUpperCase();
    }
  }

  const source = new Readable({
    objectMode: true,
    read() {
      this.push('a');
      this.push('b');
      this.push('c');
      this.push(null);
    }
  });

  const stream = Readable.from(generate(source));

  const expected = ['A', 'B', 'C'];

  for await (const chunk of stream) {
    assert.strictEqual(chunk, expected.shift());
  }
}

async function endWithError() {
  async function* generate() {
    yield 1;
    yield 2;
    yield Promise.reject('Boum');
  }

  const stream = Readable.from(generate());

  const expected = [1, 2];

  try {
    for await (const chunk of stream) {
      assert.strictEqual(chunk, expected.shift());
    }
    throw new Error();
  } catch (err) {
    assert.strictEqual(expected.length, 0);
    assert.strictEqual(err, 'Boum');
  }
}

async function destroyingStreamWithErrorThrowsInGenerator() {
  const validateError = common.mustCall((e) => {
    assert.strictEqual(e, 'Boum');
  });
  async function* generate() {
    try {
      yield 1;
      yield 2;
      yield 3;
      throw new Error();
    } catch (e) {
      validateError(e);
    }
  }
  const stream = Readable.from(generate());
  stream.read();
  stream.once('error', common.mustCall());
  stream.destroy('Boum');
}

Promise.all([
  toReadableBasicSupport(),
  toReadableSyncIterator(),
  toReadablePromises(),
  toReadableString(),
  toReadableBuffer(),
  toReadableOnData(),
  toReadableOnDataNonObject(),
  destroysTheStreamWhenThrowing(),
  asTransformStream(),
  endWithError(),
  destroyingStreamWithErrorThrowsInGenerator(),
]).then(mustCall());

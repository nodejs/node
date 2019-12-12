'use strict';

const { mustCall } = require('../common');
const { once } = require('events');
const { Readable } = require('stream');
const { strictEqual } = require('assert');

async function toReadableBasicSupport() {
  async function* generate() {
    yield 'a';
    yield 'b';
    yield 'c';
  }

  const stream = Readable.from(generate());

  const expected = ['a', 'b', 'c'];

  for await (const chunk of stream) {
    strictEqual(chunk, expected.shift());
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
    strictEqual(chunk, expected.shift());
  }
}

async function toReadablePromises() {
  const promises = [
    Promise.resolve('a'),
    Promise.resolve('b'),
    Promise.resolve('c')
  ];

  const stream = Readable.from(promises);

  const expected = ['a', 'b', 'c'];

  for await (const chunk of stream) {
    strictEqual(chunk, expected.shift());
  }
}

async function toReadableString() {
  const stream = Readable.from('abc');

  const expected = ['abc'];

  for await (const chunk of stream) {
    strictEqual(chunk, expected.shift());
  }
}

async function toReadableBuffer() {
  const stream = Readable.from(Buffer.from('abc'));

  const expected = ['abc'];

  for await (const chunk of stream) {
    strictEqual(chunk.toString(), expected.shift());
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

  stream.on('data', (chunk) => {
    iterations++;
    strictEqual(chunk, expected.shift());
  });

  await once(stream, 'end');

  strictEqual(iterations, 3);
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

  stream.on('data', (chunk) => {
    iterations++;
    strictEqual(chunk instanceof Buffer, true);
    strictEqual(chunk.toString(), expected.shift());
  });

  await once(stream, 'end');

  strictEqual(iterations, 3);
}

async function destroysTheStreamWhenThrowing() {
  async function* generate() {
    throw new Error('kaboom');
  }

  const stream = Readable.from(generate());

  stream.read();

  const [err] = await once(stream, 'error');
  strictEqual(err.message, 'kaboom');
  strictEqual(stream.destroyed, true);

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
    strictEqual(chunk, expected.shift());
  }
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
  asTransformStream()
]).then(mustCall());

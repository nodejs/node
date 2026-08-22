'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable, Writable, PassThrough, pipeline } = require('stream');
const { ReadableStream, WritableStream, TransformStream } = require('stream/web');

class NonReadableNodeStream extends Writable {
  _write(chunk, encoding, callback) {
    callback();
  }
}

class NonWritableNodeStream extends Readable {
  _read() {
    this.push('x');
    this.push(null);
  }
}

function assertInvalidArg(fn, name) {
  assert.throws(fn, (err) => {
    assert.strictEqual(err.code, 'ERR_INVALID_ARG_TYPE');
    assert.strictEqual(err.message.includes(`The "${name}"`), true);
    return true;
  });
}

// Non-readable Node stream
{
  assertInvalidArg(() => {
    pipeline(
      new NonReadableNodeStream(), // source
      new PassThrough(),
      common.mustNotCall(),
    );
  }, 'source');

  assertInvalidArg(() => {
    pipeline(
      Readable.from(['x']),
      new PassThrough(),
      new NonReadableNodeStream(), // transform
      new PassThrough(),
      common.mustNotCall(),
    );
  }, 'transform[1]');

  pipeline(
    Readable.from(['x']),
    new NonReadableNodeStream(), // destination
    common.mustSucceed(),
  );
}

// Non-writable Node stream
{
  pipeline(
    new NonWritableNodeStream(), // source
    new Writable({
      write(chunk, encoding, callback) {
        callback();
      },
    }),
    common.mustSucceed(),
  );

  assertInvalidArg(() => {
    pipeline(
      Readable.from(['x']),
      new NonWritableNodeStream(), // transform
      new PassThrough(),
      common.mustNotCall(),
    );
  }, 'transform[0]');

  assertInvalidArg(() => {
    pipeline(
      Readable.from(['x']),
      new NonWritableNodeStream(), // destination
      common.mustNotCall(),
    );
  }, 'destination');
}

// ReadableStream
{
  const chunks = [];
  pipeline(
    new ReadableStream({ // source
      start(controller) {
        controller.enqueue('x');
        controller.close();
      },
    }),
    new Writable({
      write(chunk, encoding, callback) {
        chunks.push(chunk.toString());
        callback();
      },
    }),
    common.mustSucceed(() => {
      assert.deepStrictEqual(chunks, ['x']);
    }),
  );

  assertInvalidArg(() => {
    pipeline(
      Readable.from(['x']),
      new ReadableStream({ // transform
        start(controller) {
          controller.enqueue('x');
          controller.close();
        },
      }),
      new PassThrough(),
      common.mustNotCall(),
    );
  }, 'transform[0]');

  assertInvalidArg(() => {
    pipeline(
      Readable.from(['x']),
      new ReadableStream({ // destination
        start(controller) {
          controller.enqueue('x');
          controller.close();
        },
      }),
      common.mustNotCall(),
    );
  }, 'destination');
}

// TransformStream
{
  const source = new TransformStream();
  const writer = source.writable.getWriter();
  const seen = [];
  pipeline(
    source, // source
    new Writable({
      write(chunk, encoding, callback) {
        seen.push(chunk.toString());
        callback();
      },
    }),
    common.mustSucceed(() => {
      assert.deepStrictEqual(seen, ['x']);
    }),
  );

  writer.write('x')
    .then(() => writer.close())
    .then(common.mustCall());

  const transformed = [];
  pipeline(
    Readable.from(['x']),
    new TransformStream({ // transform
      transform(chunk, controller) {
        controller.enqueue(chunk.toString().toUpperCase());
      },
    }),
    new Writable({
      write(chunk, encoding, callback) {
        transformed.push(chunk.toString());
        callback();
      },
    }),
    common.mustSucceed(() => {
      assert.deepStrictEqual(transformed, ['X']);
    }),
  );

  // Destination TransformStream should not throw
  pipeline(
    Readable.from(['x']),
    new TransformStream({ // destination
      transform() {},
    }),
    () => {},
  );
}

// WritableStream
{
  assertInvalidArg(() => {
    pipeline(
      new WritableStream({ // source
        write() {},
      }),
      new PassThrough(),
      common.mustNotCall(),
    );
  }, 'source');

  assertInvalidArg(() => {
    pipeline(
      Readable.from(['x']),
      new WritableStream({ // transform
        write() {},
      }),
      new PassThrough(),
      common.mustNotCall(),
    );
  }, 'transform[0]');

  const values = [];
  pipeline(
    Readable.from(['x']),
    new WritableStream({ // destination
      write(chunk) {
        values.push(chunk.toString());
      },
    }),
    common.mustSucceed(() => {
      assert.deepStrictEqual(values, ['x']);
    }),
  );
}

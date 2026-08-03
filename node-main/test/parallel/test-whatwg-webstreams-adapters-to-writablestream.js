// Flags: --no-warnings --expose-internals

'use strict';

const common = require('../common');

const assert = require('assert');

const {
  newWritableStreamFromStreamWritable,
} = require('internal/webstreams/adapters');

const {
  Duplex,
  Writable,
  PassThrough,
} = require('stream');

class TestWritable extends Writable {
  constructor(asyncWrite = false) {
    super();
    this.chunks = [];
    this.asyncWrite = asyncWrite;
  }

  _write(chunk, encoding, callback) {
    this.chunks.push({ chunk, encoding });
    if (this.asyncWrite) {
      setImmediate(() => callback());
      return;
    }
    callback();
  }
}

[1, {}, false, []].forEach((arg) => {
  assert.throws(() => newWritableStreamFromStreamWritable(arg), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

{
  // Closing the WritableStream normally closes the stream.Writable
  // without errors.

  const writable = new TestWritable();
  writable.on('error', common.mustNotCall());
  writable.on('finish', common.mustCall());
  writable.on('close', common.mustCall());

  const writableStream = newWritableStreamFromStreamWritable(writable);

  writableStream.close().then(common.mustCall(() => {
    assert(writable.destroyed);
  }));
}

{
  // Aborting the WritableStream errors the stream.Writable

  const error = new Error('boom');
  const writable = new TestWritable();
  writable.on('error', common.mustCall((reason) => {
    assert.strictEqual(reason, error);
  }));
  writable.on('finish', common.mustNotCall());
  writable.on('close', common.mustCall());

  const writableStream = newWritableStreamFromStreamWritable(writable);

  writableStream.abort(error).then(common.mustCall(() => {
    assert(writable.destroyed);
  }));
}

{
  // Destroying the stream.Writable prematurely errors the
  // WritableStream

  const error = new Error('boom');
  const writable = new TestWritable();

  const writableStream = newWritableStreamFromStreamWritable(writable);
  assert.rejects(writableStream.close(), error).then(common.mustCall());
  writable.destroy(error);
}

{
  // Ending the stream.Writable directly errors the WritableStream
  const writable = new TestWritable();

  const writableStream = newWritableStreamFromStreamWritable(writable);

  assert.rejects(writableStream.close(), {
    code: 'ABORT_ERR'
  }).then(common.mustCall());

  writable.end();
}

{
  const writable = new TestWritable();
  const writableStream = newWritableStreamFromStreamWritable(writable);
  const writer = writableStream.getWriter();
  const ec = new TextEncoder();
  writer.write(ec.encode('hello')).then(common.mustCall(() => {
    assert.strictEqual(writable.chunks.length, 1);
    assert.deepStrictEqual(
      writable.chunks[0],
      {
        chunk: Buffer.from('hello'),
        encoding: 'buffer'
      });
  }));
}

{
  const writable = new TestWritable(true);

  writable.on('error', common.mustNotCall());
  writable.on('close', common.mustCall());
  writable.on('finish', common.mustCall());

  const writableStream = newWritableStreamFromStreamWritable(writable);
  const writer = writableStream.getWriter();
  const ec = new TextEncoder();
  writer.write(ec.encode('hello')).then(common.mustCall(() => {
    assert.strictEqual(writable.chunks.length, 1);
    assert.deepStrictEqual(
      writable.chunks[0],
      {
        chunk: Buffer.from('hello'),
        encoding: 'buffer'
      });
    writer.close().then(common.mustCall());
  }));
}

{
  const duplex = new PassThrough();
  duplex.setEncoding('utf8');
  const writableStream = newWritableStreamFromStreamWritable(duplex);
  const ec = new TextEncoder();
  writableStream
    .getWriter()
    .write(ec.encode('hello'))
    .then(common.mustCall());

  duplex.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk, 'hello');
  }));
}

{
  const writable = new Writable();
  writable.destroy();
  const writableStream = newWritableStreamFromStreamWritable(writable);
  const writer = writableStream.getWriter();
  writer.closed.then(common.mustCall());
}

{
  const duplex = new Duplex({ writable: false });
  const writableStream = newWritableStreamFromStreamWritable(duplex);
  const writer = writableStream.getWriter();
  writer.closed.then(common.mustCall());
}

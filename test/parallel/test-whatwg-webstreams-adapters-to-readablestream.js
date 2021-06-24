// Flags: --no-warnings --expose-internals
'use strict';

const common = require('../common');

const assert = require('assert');

const {
  newReadableStreamFromStreamReadable,
} = require('internal/webstreams/adapters');

const {
  Duplex,
  Readable,
} = require('stream');

const {
  kState,
} = require('internal/webstreams/util');

{
  // Canceling the readableStream closes the readable.
  const readable = new Readable({
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  readable.on('close', common.mustCall());
  readable.on('end', common.mustNotCall());
  readable.on('pause', common.mustCall());
  readable.on('resume', common.mustNotCall());
  readable.on('error', common.mustCall((error) => {
    assert.strictEqual(error.code, 'ABORT_ERR');
  }));

  const readableStream = newReadableStreamFromStreamReadable(readable);

  readableStream.cancel().then(common.mustCall());
}

{
  // Prematurely destroying the stream.Readable without an error
  // closes the ReadableStream with a premature close error but does
  // not error the readable.

  const readable = new Readable({
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  const readableStream = newReadableStreamFromStreamReadable(readable);

  assert(!readableStream.locked);

  const reader = readableStream.getReader();

  assert.rejects(reader.closed, {
    code: 'ERR_STREAM_PREMATURE_CLOSE',
  });

  readable.on('end', common.mustNotCall());
  readable.on('error', common.mustNotCall());

  readable.on('close', common.mustCall(() => {
    assert.strictEqual(readableStream[kState].state, 'errored');
  }));

  readable.destroy();
}

{
  // Ending the readable without an error just closes the
  // readableStream without an error.
  const readable = new Readable({
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  const readableStream = newReadableStreamFromStreamReadable(readable);

  assert(!readableStream.locked);

  const reader = readableStream.getReader();

  reader.closed.then(common.mustCall());

  readable.on('end', common.mustCall());
  readable.on('error', common.mustNotCall());

  readable.on('close', common.mustCall(() => {
    assert.strictEqual(readableStream[kState].state, 'closed');
  }));

  readable.push(null);
}

{
  // Destroying the readable with an error should error the readableStream
  const error = new Error('boom');
  const readable = new Readable({
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  const readableStream = newReadableStreamFromStreamReadable(readable);

  assert(!readableStream.locked);

  const reader = readableStream.getReader();

  assert.rejects(reader.closed, error);

  readable.on('end', common.mustNotCall());
  readable.on('error', common.mustCall((reason) => {
    assert.strictEqual(reason, error);
  }));

  readable.on('close', common.mustCall(() => {
    assert.strictEqual(readableStream[kState].state, 'errored');
  }));

  readable.destroy(error);
}

{
  const readable = new Readable({
    encoding: 'utf8',
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  const readableStream = newReadableStreamFromStreamReadable(readable);
  const reader = readableStream.getReader();

  readable.on('data', common.mustCall());
  readable.on('end', common.mustCall());
  readable.on('close', common.mustCall());

  (async () => {
    assert.deepStrictEqual(
      await reader.read(),
      { value: 'hello', done: false });
    assert.deepStrictEqual(
      await reader.read(),
      { value: undefined, done: true });

  })().then(common.mustCall());
}

{
  const data = {};
  const readable = new Readable({
    objectMode: true,
    read() {
      readable.push(data);
      readable.push(null);
    }
  });

  assert(readable.readableObjectMode);

  const readableStream = newReadableStreamFromStreamReadable(readable);
  const reader = readableStream.getReader();

  readable.on('data', common.mustCall());
  readable.on('end', common.mustCall());
  readable.on('close', common.mustCall());

  (async () => {
    assert.deepStrictEqual(
      await reader.read(),
      { value: data, done: false });
    assert.deepStrictEqual(
      await reader.read(),
      { value: undefined, done: true });

  })().then(common.mustCall());
}

{
  const readable = new Readable();
  readable.destroy();
  const readableStream = newReadableStreamFromStreamReadable(readable);
  const reader = readableStream.getReader();
  reader.closed.then(common.mustCall());
}

{
  const duplex = new Duplex({ readable: false });
  duplex.destroy();
  const readableStream = newReadableStreamFromStreamReadable(duplex);
  const reader = readableStream.getReader();
  reader.closed.then(common.mustCall());
}

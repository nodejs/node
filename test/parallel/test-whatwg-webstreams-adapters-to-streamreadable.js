// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');

const assert = require('assert');

const {
  pipeline,
  finished,
  Writable,
} = require('stream');

const {
  ReadableStream,
  WritableStream,
} = require('stream/web');

const {
  newStreamReadableFromReadableStream,
} = require('internal/webstreams/adapters');

const {
  kState,
} = require('internal/webstreams/util');

class MySource {
  constructor(value = new Uint8Array(10)) {
    this.value = value;
  }

  start(c) {
    this.started = true;
    this.controller = c;
  }

  pull(controller) {
    controller.enqueue(this.value);
    controller.close();
  }

  cancel(reason) {
    this.canceled = true;
    this.cancelReason = reason;
  }
}

{
  // Destroying the readable without an error closes
  // the readableStream.

  const readableStream = new ReadableStream();
  const readable = newStreamReadableFromReadableStream(readableStream);

  assert(readableStream.locked);

  assert.rejects(readableStream.cancel(), {
    code: 'ERR_INVALID_STATE',
  });
  assert.rejects(readableStream.pipeTo(new WritableStream()), {
    code: 'ERR_INVALID_STATE',
  });
  assert.throws(() => readableStream.tee(), {
    code: 'ERR_INVALID_STATE',
  });
  assert.throws(() => readableStream.getReader(), {
    code: 'ERR_INVALID_STATE',
  });
  assert.throws(() => {
    readableStream.pipeThrough({
      readable: new ReadableStream(),
      writable: new WritableStream(),
    });
  }, {
    code: 'ERR_INVALID_STATE',
  });

  readable.destroy();

  readable.on('close', common.mustCall(() => {
    assert.strictEqual(readableStream[kState].state, 'closed');
  }));
}

{
  // Destroying the readable with an error closes the readableStream
  // without error but records the cancel reason in the source.
  const error = new Error('boom');
  const source = new MySource();
  const readableStream = new ReadableStream(source);
  const readable = newStreamReadableFromReadableStream(readableStream);

  assert(readableStream.locked);

  readable.destroy(error);

  readable.on('error', common.mustCall((reason) => {
    assert.strictEqual(reason, error);
  }));

  readable.on('close', common.mustCall(() => {
    assert.strictEqual(readableStream[kState].state, 'closed');
    assert.strictEqual(source.cancelReason, error);
  }));
}

{
  // An error in the source causes the readable to error.
  const error = new Error('boom');
  const source = new MySource();
  const readableStream = new ReadableStream(source);
  const readable = newStreamReadableFromReadableStream(readableStream);

  assert(readableStream.locked);

  source.controller.error(error);

  readable.on('error', common.mustCall((reason) => {
    assert.strictEqual(reason, error);
  }));

  readable.on('close', common.mustCall(() => {
    assert.strictEqual(readableStream[kState].state, 'errored');
  }));
}

{
  const readableStream = new ReadableStream(new MySource());
  const readable = newStreamReadableFromReadableStream(readableStream);

  readable.on('data', common.mustCall((chunk) => {
    assert.deepStrictEqual(chunk, Buffer.alloc(10));
  }));
  readable.on('end', common.mustCall());
  readable.on('close', common.mustCall());
  readable.on('error', common.mustNotCall());
}

{
  const readableStream = new ReadableStream(new MySource('hello'));
  const readable = newStreamReadableFromReadableStream(readableStream, {
    encoding: 'utf8',
  });

  readable.on('data', common.mustCall((chunk) => {
    assert.deepStrictEqual(chunk, 'hello');
  }));
  readable.on('end', common.mustCall());
  readable.on('close', common.mustCall());
  readable.on('error', common.mustNotCall());
}

{
  const readableStream = new ReadableStream(new MySource());
  const readable = newStreamReadableFromReadableStream(readableStream, {
    objectMode: true
  });

  readable.on('data', common.mustCall((chunk) => {
    assert.deepStrictEqual(chunk, new Uint8Array(10));
  }));
  readable.on('end', common.mustCall());
  readable.on('close', common.mustCall());
  readable.on('error', common.mustNotCall());
}

{
  const ec = new TextEncoder();
  const readable = new ReadableStream({
    start(controller) {
      controller.enqueue(ec.encode('hello'));
      setImmediate(() => {
        controller.enqueue(ec.encode('there'));
        controller.close();
      });
    }
  });
  const streamReadable = newStreamReadableFromReadableStream(readable);

  finished(streamReadable, common.mustCall());

  streamReadable.resume();
}

{
  const ec = new TextEncoder();
  const readable = new ReadableStream({
    start(controller) {
      controller.enqueue(ec.encode('hello'));
      setImmediate(() => {
        controller.enqueue(ec.encode('there'));
        controller.close();
      });
    }
  });
  const streamReadable = newStreamReadableFromReadableStream(readable);

  finished(streamReadable, common.mustCall());

  streamReadable.resume();
}

{
  const ec = new TextEncoder();
  const dc = new TextDecoder();
  const check = ['hello', 'there'];
  const readable = new ReadableStream({
    start(controller) {
      controller.enqueue(ec.encode('hello'));
      setImmediate(() => {
        controller.enqueue(ec.encode('there'));
        controller.close();
      });
    }
  });
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      assert.strictEqual(dc.decode(chunk), check.shift());
      assert.strictEqual(encoding, 'buffer');
      callback();
    }, 2),
  });

  const streamReadable = newStreamReadableFromReadableStream(readable);

  pipeline(streamReadable, writable, common.mustCall());

  streamReadable.resume();
}

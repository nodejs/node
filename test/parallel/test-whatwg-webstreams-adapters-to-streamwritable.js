// Flags: --no-warnings --expose-internals
'use strict';

const common = require('../common');

const assert = require('assert');

const {
  WritableStream,
} = require('stream/web');

const {
  newStreamWritableFromWritableStream,
} = require('internal/webstreams/adapters');

const {
  finished,
  pipeline,
  Readable,
} = require('stream');

const {
  kState,
} = require('internal/webstreams/util');

class TestSource {
  constructor() {
    this.chunks = [];
  }

  start(c) {
    this.controller = c;
    this.started = true;
  }

  write(chunk) {
    this.chunks.push(chunk);
  }

  close() {
    this.closed = true;
  }

  abort(reason) {
    this.abortReason = reason;
  }
}

[1, {}, false, []].forEach((arg) => {
  assert.throws(() => newStreamWritableFromWritableStream(arg), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

{
  // Ending the stream.Writable should close the writableStream
  const source = new TestSource();
  const writableStream = new WritableStream(source);
  const writable = newStreamWritableFromWritableStream(writableStream);

  assert(writableStream.locked);

  writable.end('chunk');

  writable.on('close', common.mustCall(() => {
    assert(writableStream.locked);
    assert.strictEqual(writableStream[kState].state, 'closed');
    assert.strictEqual(source.chunks.length, 1);
    assert.deepStrictEqual(source.chunks[0], Buffer.from('chunk'));
  }));
}

{
  // Destroying the stream.Writable without an error should close
  // the writableStream with no error.
  const source = new TestSource();
  const writableStream = new WritableStream(source);
  const writable = newStreamWritableFromWritableStream(writableStream);

  assert(writableStream.locked);

  writable.destroy();

  writable.on('close', common.mustCall(() => {
    assert(writableStream.locked);
    assert.strictEqual(writableStream[kState].state, 'closed');
    assert.strictEqual(source.chunks.length, 0);
  }));
}

{
  // Destroying the stream.Writable with an error should error
  // the writableStream
  const error = new Error('boom');
  const source = new TestSource();
  const writableStream = new WritableStream(source);
  const writable = newStreamWritableFromWritableStream(writableStream);

  assert(writableStream.locked);

  writable.destroy(error);

  writable.on('error', common.mustCall((reason) => {
    assert.strictEqual(reason, error);
  }));

  writable.on('close', common.mustCall(() => {
    assert(writableStream.locked);
    assert.strictEqual(writableStream[kState].state, 'errored');
    assert.strictEqual(writableStream[kState].storedError, error);
    assert.strictEqual(source.chunks.length, 0);
  }));
}

{
  // Attempting to close, abort, or getWriter on writableStream
  // should fail because it is locked. An internal error in
  // writableStream should error the writable.
  const error = new Error('boom');
  const source = new TestSource();
  const writableStream = new WritableStream(source);
  const writable = newStreamWritableFromWritableStream(writableStream);

  assert(writableStream.locked);

  assert.rejects(writableStream.close(), {
    code: 'ERR_INVALID_STATE',
  });

  assert.rejects(writableStream.abort(), {
    code: 'ERR_INVALID_STATE',
  });

  assert.throws(() => writableStream.getWriter(), {
    code: 'ERR_INVALID_STATE',
  });

  writable.on('error', common.mustCall((reason) => {
    assert.strictEqual(error, reason);
  }));

  source.controller.error(error);
}

{
  const source = new TestSource();
  const writableStream = new WritableStream(source);
  const writable = newStreamWritableFromWritableStream(writableStream);

  writable.on('error', common.mustNotCall());
  writable.on('finish', common.mustCall());
  writable.on('close', common.mustCall(() => {
    assert.strictEqual(source.chunks.length, 1);
    assert.deepStrictEqual(source.chunks[0], Buffer.from('hello'));
  }));

  writable.write('hello', common.mustCall());
  writable.end();
}

{
  const source = new TestSource();
  const writableStream = new WritableStream(source);
  const writable =
    newStreamWritableFromWritableStream(writableStream, {
      decodeStrings: false,
    });

  writable.on('error', common.mustNotCall());
  writable.on('finish', common.mustCall());
  writable.on('close', common.mustCall(() => {
    assert.strictEqual(source.chunks.length, 1);
    assert.deepStrictEqual(source.chunks[0], 'hello');
  }));

  writable.write('hello', common.mustCall());
  writable.end();
}

{
  const source = new TestSource();
  const writableStream = new WritableStream(source);
  const writable =
    newStreamWritableFromWritableStream(
      writableStream, {
        objectMode: true
      });
  assert(writable.writableObjectMode);

  writable.on('error', common.mustNotCall());
  writable.on('finish', common.mustCall());
  writable.on('close', common.mustCall(() => {
    assert.strictEqual(source.chunks.length, 1);
    assert.strictEqual(source.chunks[0], 'hello');
  }));

  writable.write('hello', common.mustCall());
  writable.end();
}

{
  const writableStream = new WritableStream({
    write: common.mustCall(2),
    close: common.mustCall(),
  });
  const writable = newStreamWritableFromWritableStream(writableStream);

  finished(writable, common.mustCall());

  writable.write('hello');
  writable.write('world');
  writable.end();
}

{
  const writableStream = new WritableStream({
    write: common.mustCall(2),
    close: common.mustCall(),
  });
  const writable = newStreamWritableFromWritableStream(writableStream);

  const readable = new Readable({
    read() {
      readable.push(Buffer.from('hello'));
      readable.push(Buffer.from('world'));
      readable.push(null);
    }
  });

  pipeline(readable, writable, common.mustCall());
}

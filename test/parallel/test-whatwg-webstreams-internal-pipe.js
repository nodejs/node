// Flags: --expose-internals --no-warnings
'use strict';

// Tests for the internal StreamBase pipe optimization infrastructure
// described in nodejs/performance#134
//
// Note(mertcanaltin): Full fast-path testing requires real StreamBase implementations
// (like HTTP/2 streams or TCP sockets), not JSStream mocks.
// These tests verify the marker attachment and fallback behavior.

const common = require('../common');

const assert = require('assert');

const {
  internalBinding,
} = require('internal/test/binding');

const {
  newWritableStreamFromStreamBase,
  newReadableStreamFromStreamBase,
} = require('internal/webstreams/adapters');

const {
  kStreamBase,
} = require('internal/webstreams/util');

const {
  JSStream,
} = internalBinding('js_stream');

// Test 1: kStreamBase marker is attached to ReadableStream
{
  const stream = new JSStream();
  const readable = newReadableStreamFromStreamBase(stream);

  assert.strictEqual(readable[kStreamBase], stream);

  // Cleanup
  stream.emitEOF();
}

// Test 2: kStreamBase marker is attached to WritableStream
{
  const stream = new JSStream();
  stream.onwrite = common.mustNotCall();
  stream.onshutdown = (req) => req.oncomplete();

  const writable = newWritableStreamFromStreamBase(stream);

  assert.strictEqual(writable[kStreamBase], stream);

  // Cleanup
  writable.close();
}

// Test 3: Regular JS streams don't have kStreamBase
{
  const { ReadableStream, WritableStream } = require('stream/web');

  const rs = new ReadableStream({
    pull(controller) {
      controller.enqueue('chunk');
      controller.close();
    },
  });

  const ws = new WritableStream({
    write() {},
  });

  assert.strictEqual(rs[kStreamBase], undefined);
  assert.strictEqual(ws[kStreamBase], undefined);

  // Pipe should still work (standard path)
  rs.pipeTo(ws).then(common.mustCall());
}

// Test 4: Mixed streams (one internal, one JS) use standard path
{
  const stream = new JSStream();
  stream.onshutdown = (req) => req.oncomplete();
  const readable = newReadableStreamFromStreamBase(stream);

  const { WritableStream } = require('stream/web');
  const chunks = [];
  const ws = new WritableStream({
    write(chunk) {
      chunks.push(chunk);
    },
  });

  // Readable has kStreamBase, ws does not - should use standard path
  assert.ok(readable[kStreamBase]);
  assert.strictEqual(ws[kStreamBase], undefined);

  const pipePromise = readable.pipeTo(ws);

  stream.readBuffer(Buffer.from('hello'));
  stream.emitEOF();

  pipePromise.then(common.mustCall(() => {
    assert.strictEqual(chunks.length, 1);
  }));
}

// Test 5: Verify kStreamBase is the correct symbol from util
{
  const {
    kStreamBase: kStreamBase2,
  } = require('internal/webstreams/util');

  // Should be the same symbol
  assert.strictEqual(kStreamBase, kStreamBase2);
}

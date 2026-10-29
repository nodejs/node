// Flags: --expose-internals --no-warnings
'use strict';

// Exercises writableStreamDefaultWriterWriteWithRequest: settle paths
// and the latched precondition failures.

const common = require('../common');
const assert = require('assert');

const {
  WritableStream,
  WritableStreamDefaultWriter,
  ReadableStream,
} = require('stream/web');

const {
  writableStreamDefaultWriterWriteWithRequest,
} = require('internal/webstreams/writablestream');

function makeRequest(overrides = {}) {
  return {
    promise: null,
    pending: 0,
    failed: false,
    failure: undefined,
    resolve: common.mustNotCall('resolve'),
    reject: common.mustNotCall('reject'),
    ...overrides,
  };
}

{
  // A write to a writable destination bumps `pending` and settles through
  // request.resolve().
  const request = makeRequest({
    resolve: common.mustCall(function() {
      assert.strictEqual(this, request);
      assert.strictEqual(this.pending, 1);
      assert.strictEqual(this.failed, false);
    }),
  });
  const ws = new WritableStream({
    write: common.mustCall((chunk) => {
      assert.strictEqual(chunk, 'chunk');
    }),
  });
  const writer = new WritableStreamDefaultWriter(ws);
  writableStreamDefaultWriterWriteWithRequest(writer, 'chunk', request);
  assert.strictEqual(request.pending, 1);
}

{
  // A write whose sink rejects settles through request.reject() with the
  // sink's error.
  const error = new Error('sink failure');
  const request = makeRequest({
    reject: common.mustCall(function(reason) {
      assert.strictEqual(this, request);
      assert.strictEqual(reason, error);
    }),
  });
  const ws = new WritableStream({
    write: common.mustCall(() => Promise.reject(error)),
  });
  const writer = new WritableStreamDefaultWriter(ws);
  writableStreamDefaultWriterWriteWithRequest(writer, 'chunk', request);
  assert.strictEqual(request.pending, 1);
}

{
  // Writing to an errored destination latches the stored error without
  // queueing the write.
  const error = new Error('start failure');
  const ws = new WritableStream({
    start(controller) { controller.error(error); },
  });
  queueMicrotask(common.mustCall(() => {
    const writer = new WritableStreamDefaultWriter(ws);
    const request = makeRequest();
    writableStreamDefaultWriterWriteWithRequest(writer, 'chunk', request);
    assert.strictEqual(request.pending, 0);
    assert.strictEqual(request.failed, true);
    assert.strictEqual(request.failure, error);
  }));
}

{
  // Writing to a destination with a queued close latches an
  // ERR_INVALID_STATE TypeError.
  const ws = new WritableStream({});
  const writer = new WritableStreamDefaultWriter(ws);
  writer.close().then(common.mustCall());
  const request = makeRequest();
  writableStreamDefaultWriterWriteWithRequest(writer, 'chunk', request);
  assert.strictEqual(request.pending, 0);
  assert.strictEqual(request.failed, true);
  assert.match(request.failure.message, /WritableStream is closed/);
}

{
  // Writing to an erroring destination latches the abort reason. The
  // stream stays in the 'erroring' state until its start algorithm
  // settles, so aborting right after construction reaches it
  // deterministically.
  const reason = new Error('abort reason');
  const ws = new WritableStream({});
  const writer = new WritableStreamDefaultWriter(ws);
  writer.abort(reason).then(common.mustCall());
  const request = makeRequest();
  writableStreamDefaultWriterWriteWithRequest(writer, 'chunk', request);
  assert.strictEqual(request.pending, 0);
  assert.strictEqual(request.failed, true);
  assert.strictEqual(request.failure, reason);
}

{
  // A size algorithm that detaches the writer makes the write latch a
  // mismatched-streams error.
  let writer;
  const ws = new WritableStream({}, {
    size: common.mustCall(() => {
      writer.releaseLock();
      return 1;
    }),
    highWaterMark: 1,
  });
  writer = new WritableStreamDefaultWriter(ws);
  const request = makeRequest();
  writableStreamDefaultWriterWriteWithRequest(writer, 'chunk', request);
  assert.strictEqual(request.pending, 0);
  assert.strictEqual(request.failed, true);
  assert.match(request.failure.message, /Mismatched WritableStreams/);
}

{
  // End to end: aborting a pipe with a write in flight still waits for
  // the in-flight write before aborting the destination, and the pipe
  // rejects with an AbortError.
  const ac = new AbortController();
  const order = [];
  const { promise: gate, resolve: openGate } = Promise.withResolvers();
  let i = 0;
  const rs = new ReadableStream({
    pull(controller) { controller.enqueue(i++); },
  });
  const pipe = rs.pipeTo(new WritableStream({
    write: common.mustCall((chunk) => {
      order.push(`write:${chunk}`);
      ac.abort();
      queueMicrotask(() => {
        order.push('settle:0');
        openGate();
      });
      return gate;
    }),
    abort: common.mustCall(() => {
      order.push('abort');
      assert.deepStrictEqual(order, ['write:0', 'settle:0', 'abort']);
    }),
  }), { signal: ac.signal });
  assert.rejects(pipe, { name: 'AbortError' }).then(common.mustCall());
}

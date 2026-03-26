'use strict';

const common = require('../common');
const { finished, addAbortSignal } = require('stream');
const { ReadableStream, WritableStream } = require('stream/web');
const assert = require('assert');
const { setImmediate } = require('timers/promises');

const typeErrorPredicate = { name: 'TypeError', code: 'ERR_INVALID_STATE' };

function createTestReadableStream() {
  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
      controller.enqueue('a');
      controller.enqueue('b');
      controller.enqueue('c');
      controller.close();
    }
  });
  return [rs, controller];
}

function createTestWritableStream(values) {
  let controller;
  const ws = new WritableStream({
    start(c) { controller = c; },
    write(chunk, c) {
      values.push(chunk);
    }
  });
  return [ ws, controller ];
}

/**
 *
 * @param {ReadableStream} rs
 * @param {import('internal/webstreams/readablestream').ReadableStreamReader} reader
 * @param {{
 *   isByob?: boolean,
 *   controller?: import('internal/webstreams/readablestream').ReadableStreamController,
 *   additionalAssertions?: () => void,
 * }} options
 */
function assertReadableStreamEventuallyAborted(rs, reader, {
  isByob,
  controller,
  additionalAssertions = common.mustCall()
} = {}) {
  finished(rs, { writable: false }, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.rejects(reader.read(...(isByob ? [new Uint8Array(1)] : [])), /AbortError/).then(common.mustCall());
    assert.rejects(reader.closed, /AbortError/).then(common.mustCall());
    if (controller) {
      assert.throws(() => controller.close(), typeErrorPredicate);
      assert.throws(() => controller.enqueue(isByob ? new Uint8Array(1) : 'a'), typeErrorPredicate);
      controller.error(new Error()); // Never throws
    }
    additionalAssertions();
  }));
}

/**
 *
 * @param {WritableStream} ws
 * @param {import('internal/webstreams/writablestream').WritableStreamDefaultWriter} writer
 * @param {{
 *   controller?: import('internal/webstreams/writablestream').WritableStreamDefaultController,
 *   additionalAssertions?: () => void,
 * }} options
 */
function assertWritableStreamEventuallyAborted(ws, writer, {
  controller,
  additionalAssertions = common.mustCall(),
} = {}) {
  finished(ws, { readable: false }, common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    assert.rejects(writer.write('a'), /AbortError/).then(common.mustCall());
    assert.rejects(writer.closed, /AbortError/).then(common.mustCall());
    if (controller) {
      controller.error(new Error()); // Never throws
      assert.strictEqual(controller.signal.aborted, true);
    }
    additionalAssertions();
  }));
}

{
  const [rs, controller] = createTestReadableStream();

  const reader = rs.getReader();

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);

  assertReadableStreamEventuallyAborted(rs, reader, { controller });

  reader.read().then(common.mustCall((result) => {
    assert.strictEqual(result.value, 'a');
    ac.abort();
  }));
}

{
  const [rs] = createTestReadableStream();

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);

  assert.rejects((async () => {
    for await (const chunk of rs) {
      if (chunk === 'b') {
        ac.abort();
      }
    }
  })(), /AbortError/).then(common.mustCall());
}

{
  const [rs, controller] = createTestReadableStream();
  const reader = rs.getReader();
  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);
  controller.error = common.mustNotCall(
    'addAbortSignal() must not call an overridden controller.error()');

  assertReadableStreamEventuallyAborted(rs, reader);

  reader.read().then(common.mustCall(() => {
    ac.abort();
  }));
}

{
  let controller;
  const rs = new ReadableStream({
    type: 'bytes',
    start(c) { controller = c; },
  });
  const ac = new AbortController();
  addAbortSignal(ac.signal, rs);

  const reader = rs.getReader({ mode: 'byob' });
  assertReadableStreamEventuallyAborted(rs, reader, { controller, isByob: true });

  ac.abort();
}

{
  /** @member {import('internal/webstreams/readablestream').ReadableByteStreamController} */
  let controller;
  const rs = new ReadableStream({
    type: 'bytes',
    start(c) { controller = c; },
  });
  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);
  controller.error = common.mustNotCall('addAbortSignal() must not call an overridden controller.error()');

  const reader = rs.getReader({ mode: 'byob' });
  assertReadableStreamEventuallyAborted(rs, reader, { isByob: true });

  ac.abort();
}

{
  /** @member {import('internal/webstreams/readablestream').ReadableStreamDefaultController} */
  let controller;
  let pullPromiseWithResolvers = Promise.withResolvers();
  const rs = new ReadableStream({
    start(c) { controller = c; },
    pull() { return pullPromiseWithResolvers.promise; },
  });
  const ac = new AbortController();
  addAbortSignal(ac.signal, rs);

  const reader = rs.getReader();
  assertReadableStreamEventuallyAborted(rs, reader);

  const readPromise = reader.read();
  pullPromiseWithResolvers.resolve(setImmediate().then(() => {
    ac.abort();
    controller.enqueue('a');
  }));
  assert.rejects(readPromise, /AbortError/).then(common.mustCall());
  assert.rejects(pullPromiseWithResolvers.promise, typeErrorPredicate).then(common.mustCall());
}

{
  const [rs1, controller1] = createTestReadableStream();
  const [rs2, controller2] = createTestReadableStream();

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs1);
  addAbortSignal(ac.signal, rs2);

  const reader1 = rs1.getReader();
  const reader2 = rs2.getReader();

  assertReadableStreamEventuallyAborted(rs1, reader1, { controller: controller1 });
  assertReadableStreamEventuallyAborted(rs2, reader2, { controller: controller2 });

  ac.abort();
}

{
  const [rs, controller] = createTestReadableStream();

  const { 0: rs1, 1: rs2 } = rs.tee();

  const ac = new AbortController();

  addAbortSignal(ac.signal, rs);

  const reader1 = rs1.getReader();
  const reader2 = rs2.getReader();

  assertReadableStreamEventuallyAborted(rs1, reader1, { controller });
  assertReadableStreamEventuallyAborted(rs2, reader2, { controller });

  ac.abort();
}

{
  const values = [];
  const [ws, controller] = createTestWritableStream(values);

  const ac = new AbortController();

  addAbortSignal(ac.signal, ws);

  const writer = ws.getWriter();

  assertWritableStreamEventuallyAborted(ws, writer, {
    controller,
    additionalAssertions: common.mustCall(() => {
      assert.deepStrictEqual(values, ['a']);
    }),
  });

  writer.write('a')
    .then(common.mustCall(() => { ac.abort(); }));
}

{
  const values = [];

  const [ws1, controller1] = createTestWritableStream(values);
  const [ws2, controller2] = createTestWritableStream(values);

  const ac = new AbortController();

  addAbortSignal(ac.signal, ws1);
  addAbortSignal(ac.signal, ws2);

  const writer1 = ws1.getWriter();
  const writer2 = ws2.getWriter();

  const additionalAssertions = common.mustCall(() => {
    assert.deepStrictEqual(values, []);
  }, 2);
  assertWritableStreamEventuallyAborted(ws1, writer1, { controller: controller1, additionalAssertions });
  assertWritableStreamEventuallyAborted(ws2, writer2, { controller: controller2, additionalAssertions });

  ac.abort();
}

{
  /** @member {import('internal/webstreams/writablestream').WritableStreamDefaultController} */
  let controller;
  const ws = new WritableStream({
    start(c) { controller = c; },
  });
  const ac = new AbortController();
  addAbortSignal(ac.signal, ws);

  controller.abort = common.mustNotCall('addAbortSignal() must not call an overridden controller.abort()');
  controller.error = common.mustNotCall('addAbortSignal() must not call an overridden controller.error()');

  const writer = ws.getWriter();
  assertWritableStreamEventuallyAborted(ws, writer);

  ac.abort();
}

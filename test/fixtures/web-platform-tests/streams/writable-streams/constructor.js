'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
  self.importScripts('../resources/constructor-ordering.js');
}

const error1 = new Error('error1');
error1.name = 'error1';

promise_test(() => {
  let controller;
  const ws = new WritableStream({
    start(c) {
      controller = c;
    }
  });

  // Now error the stream after its construction.
  controller.error(error1);

  const writer = ws.getWriter();

  assert_equals(writer.desiredSize, null, 'desiredSize should be null');
  return writer.closed.catch(r => {
    assert_equals(r, error1, 'ws should be errored by the passed error');
  });
}, 'controller argument should be passed to start method');

promise_test(t => {
  const ws = new WritableStream({
    write(chunk, controller) {
      controller.error(error1);
    }
  });

  const writer = ws.getWriter();

  return Promise.all([
    writer.write('a'),
    promise_rejects(t, error1, writer.closed, 'controller.error() in write() should error the stream')
  ]);
}, 'controller argument should be passed to write method');

// Older versions of the standard had the controller argument passed to close(). It wasn't useful, and so has been
// removed. This test remains to identify implementations that haven't been updated.
promise_test(t => {
  const ws = new WritableStream({
    close(...args) {
      t.step(() => {
        assert_array_equals(args, [], 'no arguments should be passed to close');
      });
    }
  });

  return ws.getWriter().close();
}, 'controller argument should not be passed to close method');

promise_test(() => {
  const ws = new WritableStream({}, {
    highWaterMark: 1000,
    size() { return 1; }
  });

  const writer = ws.getWriter();

  assert_equals(writer.desiredSize, 1000, 'desiredSize should be 1000');
  return writer.ready.then(v => {
    assert_equals(v, undefined, 'ready promise should fulfill with undefined');
  });
}, 'highWaterMark should be reflected to desiredSize');

promise_test(() => {
  const ws = new WritableStream({}, {
    highWaterMark: Infinity,
    size() { return 0; }
  });

  const writer = ws.getWriter();

  assert_equals(writer.desiredSize, Infinity, 'desiredSize should be Infinity');

  return writer.ready;
}, 'WritableStream should be writable and ready should fulfill immediately if the strategy does not apply ' +
    'backpressure');

test(() => {
  new WritableStream();
}, 'WritableStream should be constructible with no arguments');

test(() => {
  const ws = new WritableStream({});

  const writer = ws.getWriter();

  assert_equals(typeof writer.write, 'function', 'writer should have a write method');
  assert_equals(typeof writer.abort, 'function', 'writer should have an abort method');
  assert_equals(typeof writer.close, 'function', 'writer should have a close method');

  assert_equals(writer.desiredSize, 1, 'desiredSize should start at 1');

  assert_not_equals(typeof writer.ready, 'undefined', 'writer should have a ready property');
  assert_equals(typeof writer.ready.then, 'function', 'ready property should be thenable');
  assert_not_equals(typeof writer.closed, 'undefined', 'writer should have a closed property');
  assert_equals(typeof writer.closed.then, 'function', 'closed property should be thenable');
}, 'WritableStream instances should have standard methods and properties');

test(() => {
  ['WritableStreamDefaultWriter', 'WritableStreamDefaultController'].forEach(c =>
      assert_equals(typeof self[c], 'undefined', `${c} should not be exported`));
}, 'private constructors should not be exported');

test(() => {
  let WritableStreamDefaultController;
  new WritableStream({
    start(c) {
      WritableStreamDefaultController = c.constructor;
    }
  });

  assert_throws(new TypeError(), () => new WritableStreamDefaultController({}),
                'constructor should throw a TypeError exception');
}, 'WritableStreamDefaultController constructor should throw');

test(() => {
  let WritableStreamDefaultController;
  const stream = new WritableStream({
    start(c) {
      WritableStreamDefaultController = c.constructor;
    }
  });

  assert_throws(new TypeError(), () => new WritableStreamDefaultController(stream),
                'constructor should throw a TypeError exception');
}, 'WritableStreamDefaultController constructor should throw when passed an initialised WritableStream');

test(() => {
  const stream = new WritableStream();
  const writer = stream.getWriter();
  const WritableStreamDefaultWriter = writer.constructor;
  writer.releaseLock();
  assert_throws(new TypeError(), () => new WritableStreamDefaultWriter({}),
                'constructor should throw a TypeError exception');
}, 'WritableStreamDefaultWriter should throw unless passed a WritableStream');

test(() => {
  const stream = new WritableStream();
  const writer = stream.getWriter();
  const WritableStreamDefaultWriter = writer.constructor;
  assert_throws(new TypeError(), () => new WritableStreamDefaultWriter(stream),
                'constructor should throw a TypeError exception');
}, 'WritableStreamDefaultWriter constructor should throw when stream argument is locked');

const operations = [
  op('get', 'size'),
  op('get', 'highWaterMark'),
  op('get', 'type'),
  op('validate', 'type'),
  op('validate', 'size'),
  op('tonumber', 'highWaterMark'),
  op('validate', 'highWaterMark'),
  op('get', 'write'),
  op('validate', 'write'),
  op('get', 'close'),
  op('validate', 'close'),
  op('get', 'abort'),
  op('validate', 'abort'),
  op('get', 'start'),
  op('validate', 'start')
];

for (const failureOp of operations) {
  test(() => {
    const record = new OpRecorder(failureOp);
    const underlyingSink = createRecordingObjectWithProperties(record, ['type', 'start', 'write', 'close', 'abort']);
    const strategy = createRecordingStrategy(record);

    try {
      new WritableStream(underlyingSink, strategy);
      assert_unreached('constructor should throw');
    } catch (e) {
      assert_equals(typeof e, 'object', 'e should be an object');
    }

    assert_equals(record.actual(), expectedAsString(operations, failureOp),
                  'operations should be performed in the right order');
  }, `WritableStream constructor should stop after ${failureOp} fails`);
}

done();

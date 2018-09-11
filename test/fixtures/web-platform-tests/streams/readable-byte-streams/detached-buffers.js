'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
}

promise_test(() => {
  const stream = new ReadableStream({
    start(c) {
      c.close();
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });
  const view = new Uint8Array([1, 2, 3]);
  return reader.read(view).then(({ value, done }) => {
    // Sanity checks
    assert_true(value instanceof Uint8Array, 'The value read must be a Uint8Array');
    assert_not_equals(value, view, 'The value read must not be the *same* Uint8Array');
    assert_array_equals(value, [], 'The value read must be an empty Uint8Array, since the stream is closed');
    assert_true(done, 'done must be true, since the stream is closed');

    // The important assertions
    assert_not_equals(value.buffer, view.buffer, 'a different ArrayBuffer must underlie the value');
    assert_equals(view.buffer.byteLength, 0, 'the original buffer must be detached');
  });
}, 'ReadableStream with byte source: read()ing from a closed stream still transfers the buffer');

promise_test(() => {
  const stream = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array([1, 2, 3]));
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });
  const view = new Uint8Array([4, 5, 6]);
  return reader.read(view).then(({ value, done }) => {
    // Sanity checks
    assert_true(value instanceof Uint8Array, 'The value read must be a Uint8Array');
    assert_not_equals(value, view, 'The value read must not be the *same* Uint8Array');
    assert_array_equals(value, [1, 2, 3], 'The value read must be the enqueued Uint8Array, not the original values');
    assert_false(done, 'done must be false, since the stream is not closed');

    // The important assertions
    assert_not_equals(value.buffer, view.buffer, 'a different ArrayBuffer must underlie the value');
    assert_equals(view.buffer.byteLength, 0, 'the original buffer must be detached');
  });
}, 'ReadableStream with byte source: read()ing from a stream with queued chunks still transfers the buffer');

test(() => {
  const stream = new ReadableStream({
    start(c) {
      const view = new Uint8Array([1, 2, 3]);
      c.enqueue(view);
      assert_throws(new TypeError(), () => c.enqueue(view), 'enqueuing an already-detached buffer must throw');
    },
    type: 'bytes'
  });
}, 'ReadableStream with byte source: enqueuing an already-detached buffer throws');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array([1, 2, 3]));
    },
    type: 'bytes'
  });
  const reader = stream.getReader({ mode: 'byob' });

  const view = new Uint8Array([4, 5, 6]);
  return reader.read(view).then(() => {
    // view is now detached
    return promise_rejects(t, new TypeError(), reader.read(view),
      'read(view) must reject when given an already-detached buffer');
  });
}, 'ReadableStream with byte source: reading into an already-detached buffer rejects');

async_test(t => {
  const stream = new ReadableStream({
    pull: t.step_func_done(c => {
      // Detach it by reading into it
      reader.read(c.byobRequest.view);

      assert_throws(new TypeError(), () => c.byobRequest.respond(1),
        'respond() must throw if the corresponding view has become detached');
    }),
    type: 'bytes'
  });
  const reader = stream.getReader({ mode: 'byob' });

  reader.read(new Uint8Array([4, 5, 6]));
}, 'ReadableStream with byte source: respond() throws if the BYOB request\'s buffer has been detached (in the ' +
   'readable state)');

async_test(t => {
  const stream = new ReadableStream({
    pull: t.step_func_done(c => {
      // Detach it by reading into it
      reader.read(c.byobRequest.view);

      c.close();

      assert_throws(new TypeError(), () => c.byobRequest.respond(0),
        'respond() must throw if the corresponding view has become detached');
    }),
    type: 'bytes'
  });
  const reader = stream.getReader({ mode: 'byob' });

  reader.read(new Uint8Array([4, 5, 6]));
}, 'ReadableStream with byte source: respond() throws if the BYOB request\'s buffer has been detached (in the ' +
   'closed state)');

async_test(t => {
  const stream = new ReadableStream({
    pull: t.step_func_done(c => {
      // Detach it by reading into it
      const view = new Uint8Array([1, 2, 3]);
      reader.read(view);

      assert_throws(new TypeError(), () => c.byobRequest.respondWithNewView(view),
        'respondWithNewView() must throw if passed a detached view');
    }),
    type: 'bytes'
  });
  const reader = stream.getReader({ mode: 'byob' });

  reader.read(new Uint8Array([4, 5, 6]));
}, 'ReadableStream with byte source: respondWithNewView() throws if the supplied view\'s buffer has been detached ' +
    '(in the readable state)');

async_test(t => {
  const stream = new ReadableStream({
    pull: t.step_func_done(c => {
      // Detach it by reading into it
      const view = new Uint8Array([1, 2, 3]);
      reader.read(view);

      c.close();

      const zeroLengthView = new Uint8Array(view.buffer, 0, 0);
      assert_throws(new TypeError(), () => c.byobRequest.respondWithNewView(zeroLengthView),
        'respondWithNewView() must throw if passed a (zero-length) view whose buffer has been detached');
    }),
    type: 'bytes'
  });
  const reader = stream.getReader({ mode: 'byob' });

  reader.read(new Uint8Array([4, 5, 6]));
}, 'ReadableStream with byte source: respondWithNewView() throws if the supplied view\'s buffer has been detached ' +
   '(in the closed state)');

done();

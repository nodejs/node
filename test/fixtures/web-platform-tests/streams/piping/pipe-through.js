'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
  self.importScripts('../resources/rs-utils.js');
  self.importScripts('../resources/test-utils.js');
}

function duckTypedPassThroughTransform() {
  let enqueueInReadable;
  let closeReadable;

  return {
    writable: new WritableStream({
      write(chunk) {
        enqueueInReadable(chunk);
      },

      close() {
        closeReadable();
      }
    }),

    readable: new ReadableStream({
      start(c) {
        enqueueInReadable = c.enqueue.bind(c);
        closeReadable = c.close.bind(c);
      }
    })
  };
}

function uninterestingReadableWritablePair() {
  return { writable: new WritableStream(), readable: new ReadableStream() };
}

promise_test(() => {
  const readableEnd = sequentialReadableStream(5).pipeThrough(duckTypedPassThroughTransform());

  return readableStreamToArray(readableEnd).then(chunks =>
    assert_array_equals(chunks, [1, 2, 3, 4, 5]), 'chunks should match');
}, 'Piping through a duck-typed pass-through transform stream should work');

promise_test(() => {
  const transform = {
    writable: new WritableStream({
      start(c) {
        c.error(new Error('this rejection should not be reported as unhandled'));
      }
    }),
    readable: new ReadableStream()
  };

  sequentialReadableStream(5).pipeThrough(transform);

  // The test harness should complain about unhandled rejections by then.
  return flushAsyncEvents();

}, 'Piping through a transform errored on the writable end does not cause an unhandled promise rejection');

test(() => {
  let calledWithArgs;
  const dummy = {
    pipeTo(...args) {
      calledWithArgs = args;

      // Does not return anything, testing the spec's guard against trying to mark [[PromiseIsHandled]] on undefined.
    }
  };

  const fakeWritable = { fake: 'writable' };
  const fakeReadable = { fake: 'readable' };
  const arg2 = { arg: 'arg2' };
  const arg3 = { arg: 'arg3' };
  const result =
    ReadableStream.prototype.pipeThrough.call(dummy, { writable: fakeWritable, readable: fakeReadable }, arg2, arg3);

  assert_array_equals(calledWithArgs, [fakeWritable, arg2],
    'The this value\'s pipeTo method should be called with the appropriate arguments');
  assert_equals(result, fakeReadable, 'return value should be the passed readable property');

}, 'pipeThrough generically calls pipeTo with the appropriate args');

test(() => {
  const dummy = {
    pipeTo() {
      return { not: 'a promise' };
    }
  };

  ReadableStream.prototype.pipeThrough.call(dummy, uninterestingReadableWritablePair());

  // Test passes if this doesn't throw or crash.

}, 'pipeThrough can handle calling a pipeTo that returns a non-promise object');

test(() => {
  const dummy = {
    pipeTo() {
      return {
        then() {},
        this: 'is not a real promise'
      };
    }
  };

  ReadableStream.prototype.pipeThrough.call(dummy, uninterestingReadableWritablePair());

  // Test passes if this doesn't throw or crash.

}, 'pipeThrough can handle calling a pipeTo that returns a non-promise thenable object');

promise_test(() => {
  const dummy = {
    pipeTo() {
      return Promise.reject(new Error('this rejection should not be reported as unhandled'));
    }
  };

  ReadableStream.prototype.pipeThrough.call(dummy, uninterestingReadableWritablePair());

  // The test harness should complain about unhandled rejections by then.
  return flushAsyncEvents();

}, 'pipeThrough should mark a real promise from a fake readable as handled');

test(() => {
  let thenCalled = false;
  let catchCalled = false;
  const dummy = {
    pipeTo() {
      const fakePromise = Object.create(Promise.prototype);
      fakePromise.then = () => {
        thenCalled = true;
      };
      fakePromise.catch = () => {
        catchCalled = true;
      };
      assert_true(fakePromise instanceof Promise, 'fakePromise fools instanceof');
      return fakePromise;
    }
  };

  // An incorrect implementation which uses an internal method to mark the promise as handled will throw or crash here.
  ReadableStream.prototype.pipeThrough.call(dummy, uninterestingReadableWritablePair());

  // An incorrect implementation that tries to mark the promise as handled by calling .then() or .catch() on the object
  // will fail these tests.
  assert_false(thenCalled, 'then should not be called');
  assert_false(catchCalled, 'catch should not be called');
}, 'pipeThrough should not be fooled by an object whose instanceof Promise returns true');

test(() => {
  const pairs = [
    {},
    { readable: undefined, writable: undefined },
    { readable: 'readable' },
    { readable: 'readable', writable: undefined },
    { writable: 'writable' },
    { readable: undefined, writable: 'writable' }
  ];
  for (let i = 0; i < pairs.length; ++i) {
    const pair = pairs[i];
    const rs = new ReadableStream();
    assert_throws(new TypeError(), () => rs.pipeThrough(pair),
                  `pipeThrough should throw for argument ${JSON.stringify(pair)} (index ${i});`);
  }
}, 'undefined readable or writable arguments should cause pipeThrough to throw');

test(() => {
  const invalidArguments = [null, 0, NaN, '', [], {}, false, () => {}];
  for (const arg of invalidArguments) {
    const rs = new ReadableStream();
    assert_equals(arg, rs.pipeThrough({ writable: new WritableStream(), readable: arg }),
                  'pipeThrough() should not throw for readable: ' + JSON.stringify(arg));
    const rs2 = new ReadableStream();
    assert_equals(rs2, rs.pipeThrough({ writable: arg, readable: rs2 }),
                  'pipeThrough() should not throw for writable: ' + JSON.stringify(arg));
  }
}, 'invalid but not undefined arguments should not cause pipeThrough to throw');

test(() => {

  const thisValue = {
    pipeTo() {
      assert_unreached('pipeTo should not be called');
    }
  };

  methodThrows(ReadableStream.prototype, 'pipeThrough', thisValue, [undefined, {}]);
  methodThrows(ReadableStream.prototype, 'pipeThrough', thisValue, [null, {}]);

}, 'pipeThrough should throw when its first argument is not convertible to an object');

test(() => {

  const args = [{ readable: {}, writable: {} }, {}];

  methodThrows(ReadableStream.prototype, 'pipeThrough', undefined, args);
  methodThrows(ReadableStream.prototype, 'pipeThrough', null, args);
  methodThrows(ReadableStream.prototype, 'pipeThrough', 1, args);
  methodThrows(ReadableStream.prototype, 'pipeThrough', { pipeTo: 'test' }, args);

}, 'pipeThrough should throw when "this" has no pipeTo method');

test(() => {
  const error = new Error('potato');

  const throwingPipeTo = {
    get pipeTo() {
      throw error;
    }
  };
  assert_throws(error,
    () => ReadableStream.prototype.pipeThrough.call(throwingPipeTo, { readable: { }, writable: { } }, {}),
    'pipeThrough should rethrow the error thrown by pipeTo');

  const thisValue = {
    pipeTo() {
      assert_unreached('pipeTo should not be called');
    }
  };

  const throwingWritable = {
    readable: {},
    get writable() {
      throw error;
    }
  };
  assert_throws(error,
    () => ReadableStream.prototype.pipeThrough.call(thisValue, throwingWritable, {}),
    'pipeThrough should rethrow the error thrown by the writable getter');

  const throwingReadable = {
    get readable() {
      throw error;
    },
    writable: {}
  };
  assert_throws(error,
    () => ReadableStream.prototype.pipeThrough.call(thisValue, throwingReadable, {}),
    'pipeThrough should rethrow the error thrown by the readable getter');

}, 'pipeThrough should rethrow errors from accessing pipeTo, readable, or writable');

test(() => {

  let count = 0;
  const thisValue = {
    pipeTo() {
      ++count;
    }
  };

  ReadableStream.prototype.pipeThrough.call(thisValue, { readable: {}, writable: {} });

  assert_equals(count, 1, 'pipeTo was called once');

}, 'pipeThrough should work with no options argument');

done();

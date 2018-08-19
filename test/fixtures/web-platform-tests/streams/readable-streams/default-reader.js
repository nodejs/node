'use strict';

if (self.importScripts) {
  self.importScripts('../resources/rs-utils.js');
  self.importScripts('/resources/testharness.js');
}

let ReadableStreamDefaultReader;

test(() => {

  // It's not exposed globally, but we test a few of its properties here.
  ReadableStreamDefaultReader = (new ReadableStream()).getReader().constructor;

}, 'Can get the ReadableStreamDefaultReader constructor indirectly');

test(() => {

  assert_throws(new TypeError(), () => new ReadableStreamDefaultReader('potato'));
  assert_throws(new TypeError(), () => new ReadableStreamDefaultReader({}));
  assert_throws(new TypeError(), () => new ReadableStreamDefaultReader());

}, 'ReadableStreamDefaultReader constructor should get a ReadableStream object as argument');

test(() => {

  const methods = ['cancel', 'constructor', 'read', 'releaseLock'];
  const properties = methods.concat(['closed']).sort();

  const rsReader = new ReadableStreamDefaultReader(new ReadableStream());
  const proto = Object.getPrototypeOf(rsReader);

  assert_array_equals(Object.getOwnPropertyNames(proto).sort(), properties);

  for (const m of methods) {
    const propDesc = Object.getOwnPropertyDescriptor(proto, m);
    assert_equals(propDesc.enumerable, false, 'method should be non-enumerable');
    assert_equals(propDesc.configurable, true, 'method should be configurable');
    assert_equals(propDesc.writable, true, 'method should be writable');
    assert_equals(typeof rsReader[m], 'function', 'should have be a method');
    const expectedName = m === 'constructor' ? 'ReadableStreamDefaultReader' : m;
    assert_equals(rsReader[m].name, expectedName, 'method should have the correct name');
  }

  const closedPropDesc = Object.getOwnPropertyDescriptor(proto, 'closed');
  assert_equals(closedPropDesc.enumerable, false, 'closed should be non-enumerable');
  assert_equals(closedPropDesc.configurable, true, 'closed should be configurable');
  assert_not_equals(closedPropDesc.get, undefined, 'closed should have a getter');
  assert_equals(closedPropDesc.set, undefined, 'closed should not have a setter');

  assert_equals(rsReader.cancel.length, 1, 'cancel has 1 parameter');
  assert_not_equals(rsReader.closed, undefined, 'has a non-undefined closed property');
  assert_equals(typeof rsReader.closed.then, 'function', 'closed property is thenable');
  assert_equals(typeof rsReader.constructor, 'function', 'has a constructor method');
  assert_equals(rsReader.constructor.length, 1, 'constructor has 1 parameter');
  assert_equals(typeof rsReader.read, 'function', 'has a getReader method');
  assert_equals(rsReader.read.length, 0, 'read has no parameters');
  assert_equals(typeof rsReader.releaseLock, 'function', 'has a releaseLock method');
  assert_equals(rsReader.releaseLock.length, 0, 'releaseLock has no parameters');

}, 'ReadableStreamDefaultReader instances should have the correct list of properties');

test(() => {

  const rsReader = new ReadableStreamDefaultReader(new ReadableStream());
  assert_equals(rsReader.closed, rsReader.closed, 'closed should return the same promise');

}, 'ReadableStreamDefaultReader closed should always return the same promise object');

test(() => {

  const rs = new ReadableStream();
  new ReadableStreamDefaultReader(rs); // Constructing directly the first time should be fine.
  assert_throws(new TypeError(), () => new ReadableStreamDefaultReader(rs),
                'constructing directly the second time should fail');

}, 'Constructing a ReadableStreamDefaultReader directly should fail if the stream is already locked (via direct ' +
   'construction)');

test(() => {

  const rs = new ReadableStream();
  new ReadableStreamDefaultReader(rs); // Constructing directly should be fine.
  assert_throws(new TypeError(), () => rs.getReader(), 'getReader() should fail');

}, 'Getting a ReadableStreamDefaultReader via getReader should fail if the stream is already locked (via direct ' +
   'construction)');

test(() => {

  const rs = new ReadableStream();
  rs.getReader(); // getReader() should be fine.
  assert_throws(new TypeError(), () => new ReadableStreamDefaultReader(rs), 'constructing directly should fail');

}, 'Constructing a ReadableStreamDefaultReader directly should fail if the stream is already locked (via getReader)');

test(() => {

  const rs = new ReadableStream();
  rs.getReader(); // getReader() should be fine.
  assert_throws(new TypeError(), () => rs.getReader(), 'getReader() should fail');

}, 'Getting a ReadableStreamDefaultReader via getReader should fail if the stream is already locked (via getReader)');

test(() => {

  const rs = new ReadableStream({
    start(c) {
      c.close();
    }
  });

  new ReadableStreamDefaultReader(rs); // Constructing directly should not throw.

}, 'Constructing a ReadableStreamDefaultReader directly should be OK if the stream is closed');

test(() => {

  const theError = new Error('don\'t say i didn\'t warn ya');
  const rs = new ReadableStream({
    start(c) {
      c.error(theError);
    }
  });

  new ReadableStreamDefaultReader(rs); // Constructing directly should not throw.

}, 'Constructing a ReadableStreamDefaultReader directly should be OK if the stream is errored');

promise_test(() => {

  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });
  const reader = rs.getReader();

  const promise = reader.read().then(result => {
    assert_object_equals(result, { value: 'a', done: false }, 'read() should fulfill with the enqueued chunk');
  });

  controller.enqueue('a');
  return promise;

}, 'Reading from a reader for an empty stream will wait until a chunk is available');

promise_test(() => {

  let cancelCalled = false;
  const passedReason = new Error('it wasn\'t the right time, sorry');
  const rs = new ReadableStream({
    cancel(reason) {
      assert_true(rs.locked, 'the stream should still be locked');
      assert_throws(new TypeError(), () => rs.getReader(), 'should not be able to get another reader');
      assert_equals(reason, passedReason, 'the cancellation reason is passed through to the underlying source');
      cancelCalled = true;
    }
  });

  const reader = rs.getReader();
  return reader.cancel(passedReason).then(() => assert_true(cancelCalled));

}, 'cancel() on a reader does not release the reader');

promise_test(() => {

  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });

  const reader = rs.getReader();
  const promise = reader.closed;

  controller.close();
  return promise;

}, 'closed should be fulfilled after stream is closed (.closed access before acquiring)');

promise_test(t => {

  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });

  const reader1 = rs.getReader();

  reader1.releaseLock();

  const reader2 = rs.getReader();
  controller.close();

  return Promise.all([
    promise_rejects(t, new TypeError(), reader1.closed),
    reader2.closed
  ]);

}, 'closed should be rejected after reader releases its lock (multiple stream locks)');

promise_test(() => {

  const rs = new ReadableStream({
    start(c) {
      c.enqueue('a');
      c.enqueue('b');
      c.close();
    }
  });

  const reader1 = rs.getReader();
  const promise1 = reader1.read().then(r => {
    assert_object_equals(r, { value: 'a', done: false }, 'reading the first chunk from reader1 works');
  });
  reader1.releaseLock();

  const reader2 = rs.getReader();
  const promise2 = reader2.read().then(r => {
    assert_object_equals(r, { value: 'b', done: false }, 'reading the second chunk from reader2 works');
  });
  reader2.releaseLock();

  return Promise.all([promise1, promise2]);

}, 'Multiple readers can access the stream in sequence');

promise_test(() => {
  const rs = new ReadableStream({
    start(c) {
      c.enqueue('a');
    }
  });

  const reader1 = rs.getReader();
  reader1.releaseLock();

  const reader2 = rs.getReader();

  // Should be a no-op
  reader1.releaseLock();

  return reader2.read().then(result => {
    assert_object_equals(result, { value: 'a', done: false },
                         'read() should still work on reader2 even after reader1 is released');
  });

}, 'Cannot use an already-released reader to unlock a stream again');

promise_test(t => {

  const rs = new ReadableStream({
    start(c) {
      c.enqueue('a');
    },
    cancel() {
      assert_unreached('underlying source cancel should not be called');
    }
  });

  const reader = rs.getReader();
  reader.releaseLock();
  const cancelPromise = reader.cancel();

  const reader2 = rs.getReader();
  const readPromise = reader2.read().then(r => {
    assert_object_equals(r, { value: 'a', done: false }, 'a new reader should be able to read a chunk');
  });

  return Promise.all([
    promise_rejects(t, new TypeError(), cancelPromise),
    readPromise
  ]);

}, 'cancel() on a released reader is a no-op and does not pass through');

promise_test(t => {

  const promiseAsserts = [];

  let controller;
  const theError = { name: 'unique error' };
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });

  const reader1 = rs.getReader();

  promiseAsserts.push(
    promise_rejects(t, theError, reader1.closed),
    promise_rejects(t, theError, reader1.read())
  );

  assert_throws(new TypeError(), () => rs.getReader(), 'trying to get another reader before erroring should throw');

  controller.error(theError);

  reader1.releaseLock();

  const reader2 = rs.getReader();

  promiseAsserts.push(
    promise_rejects(t, theError, reader2.closed),
    promise_rejects(t, theError, reader2.read())
  );

  return Promise.all(promiseAsserts);

}, 'Getting a second reader after erroring the stream and releasing the reader should succeed');

promise_test(t => {

  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });

  const promise = rs.getReader().closed.then(
    t.unreached_func('closed promise should not be fulfilled when stream is errored'),
    err => {
      assert_equals(err, undefined, 'passed error should be undefined as it was');
    }
  );

  controller.error();
  return promise;

}, 'ReadableStreamDefaultReader closed promise should be rejected with undefined if that is the error');


promise_test(t => {

  const rs = new ReadableStream({
    start() {
      return Promise.reject();
    }
  });

  return rs.getReader().read().then(
    t.unreached_func('read promise should not be fulfilled when stream is errored'),
    err => {
      assert_equals(err, undefined, 'passed error should be undefined as it was');
    }
  );

}, 'ReadableStreamDefaultReader: if start rejects with no parameter, it should error the stream with an undefined ' +
    'error');

promise_test(t => {

  const theError = { name: 'unique string' };
  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });

  const promise = promise_rejects(t, theError, rs.getReader().closed);

  controller.error(theError);
  return promise;

}, 'Erroring a ReadableStream after checking closed should reject ReadableStreamDefaultReader closed promise');

promise_test(t => {

  const theError = { name: 'unique string' };
  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });

  controller.error(theError);

  // Let's call getReader twice for extra test coverage of this code path.
  rs.getReader().releaseLock();

  return promise_rejects(t, theError, rs.getReader().closed);

}, 'Erroring a ReadableStream before checking closed should reject ReadableStreamDefaultReader closed promise');

promise_test(() => {

  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });
  const reader = rs.getReader();

  const promise = Promise.all([
    reader.read().then(result => {
      assert_object_equals(result, { value: undefined, done: true }, 'read() should fulfill with close (1)');
    }),
    reader.read().then(result => {
      assert_object_equals(result, { value: undefined, done: true }, 'read() should fulfill with close (2)');
    }),
    reader.closed
  ]);

  controller.close();
  return promise;

}, 'Reading twice on a stream that gets closed');

promise_test(() => {

  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });

  controller.close();
  const reader = rs.getReader();

  return Promise.all([
    reader.read().then(result => {
      assert_object_equals(result, { value: undefined, done: true }, 'read() should fulfill with close (1)');
    }),
    reader.read().then(result => {
      assert_object_equals(result, { value: undefined, done: true }, 'read() should fulfill with close (2)');
    }),
    reader.closed
  ]);

}, 'Reading twice on a closed stream');

promise_test(t => {

  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });

  const myError = { name: 'mashed potatoes' };
  controller.error(myError);

  const reader = rs.getReader();

  return Promise.all([
    promise_rejects(t, myError, reader.read()),
    promise_rejects(t, myError, reader.read()),
    promise_rejects(t, myError, reader.closed)
  ]);

}, 'Reading twice on an errored stream');

promise_test(t => {

  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    }
  });

  const myError = { name: 'mashed potatoes' };
  const reader = rs.getReader();

  const promise = Promise.all([
    promise_rejects(t, myError, reader.read()),
    promise_rejects(t, myError, reader.read()),
    promise_rejects(t, myError, reader.closed)
  ]);

  controller.error(myError);
  return promise;

}, 'Reading twice on a stream that gets errored');

test(() => {
  const rs = new ReadableStream();
  let toStringCalled = false;
  const mode = {
    toString() {
      toStringCalled = true;
      return '';
    }
  };
  assert_throws(new RangeError(), () => rs.getReader({ mode }), 'getReader() should throw');
  assert_true(toStringCalled, 'toString() should be called');
}, 'getReader() should call ToString() on mode');

done();

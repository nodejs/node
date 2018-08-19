'use strict';

if (self.importScripts) {
  self.importScripts('../resources/rs-utils.js');
  self.importScripts('/resources/testharness.js');
}

let ReadableStreamBYOBReader;

test(() => {

  // It's not exposed globally, but we test a few of its properties here.
  ReadableStreamBYOBReader = (new ReadableStream({ type: 'bytes' }))
      .getReader({ mode: 'byob' }).constructor;

}, 'Can get the ReadableStreamBYOBReader constructor indirectly');

test(() => {

  assert_throws(new TypeError(), () => new ReadableStreamBYOBReader('potato'));
  assert_throws(new TypeError(), () => new ReadableStreamBYOBReader({}));
  assert_throws(new TypeError(), () => new ReadableStreamBYOBReader());

}, 'ReadableStreamBYOBReader constructor should get a ReadableStream object as argument');

test(() => {

  const methods = ['cancel', 'constructor', 'read', 'releaseLock'];
  const properties = methods.concat(['closed']).sort();

  const rsReader = new ReadableStreamBYOBReader(new ReadableStream({ type: 'bytes' }));
  const proto = Object.getPrototypeOf(rsReader);

  assert_array_equals(Object.getOwnPropertyNames(proto).sort(), properties);

  for (const m of methods) {
    const propDesc = Object.getOwnPropertyDescriptor(proto, m);
    assert_equals(propDesc.enumerable, false, 'method should be non-enumerable');
    assert_equals(propDesc.configurable, true, 'method should be configurable');
    assert_equals(propDesc.writable, true, 'method should be writable');
    assert_equals(typeof rsReader[m], 'function', 'should have be a method');
    const expectedName = m === 'constructor' ? 'ReadableStreamBYOBReader' : m;
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
  assert_equals(rsReader.constructor.length, 1, 'constructor has 1 parameter');
  assert_equals(rsReader.read.length, 1, 'read has 1 parameter');
  assert_equals(rsReader.releaseLock.length, 0, 'releaseLock has no parameters');

}, 'ReadableStreamBYOBReader instances should have the correct list of properties');

promise_test(t => {

  const rs = new ReadableStream({
    pull(controller) {
      return t.step(() => {
        const byobRequest = controller.byobRequest;

        const methods = ['constructor', 'respond', 'respondWithNewView'];
        const properties = methods.concat(['view']).sort();

        const proto = Object.getPrototypeOf(byobRequest);

        assert_array_equals(Object.getOwnPropertyNames(proto).sort(), properties);

        for (const m of methods) {
          const propDesc = Object.getOwnPropertyDescriptor(proto, m);
          assert_equals(propDesc.enumerable, false, 'method should be non-enumerable');
          assert_equals(propDesc.configurable, true, 'method should be configurable');
          assert_equals(propDesc.writable, true, 'method should be writable');
          assert_equals(typeof byobRequest[m], 'function', 'should have a method');
          const expectedName = m === 'constructor' ? 'ReadableStreamBYOBRequest' : m;
          assert_equals(byobRequest[m].name, expectedName, 'method should have the correct name');
        }

        const viewPropDesc = Object.getOwnPropertyDescriptor(proto, 'view');
        assert_equals(viewPropDesc.enumerable, false, 'view should be non-enumerable');
        assert_equals(viewPropDesc.configurable, true, 'view should be configurable');
        assert_not_equals(viewPropDesc.get, undefined, 'view should have a getter');
        assert_equals(viewPropDesc.set, undefined, 'view should not have a setter');
        assert_not_equals(byobRequest.view, undefined, 'has a non-undefined view property');
        assert_equals(byobRequest.constructor.length, 0, 'constructor has 0 parameters');
        assert_equals(byobRequest.respond.length, 1, 'respond has 1 parameter');
        assert_equals(byobRequest.respondWithNewView.length, 1, 'releaseLock has 1 parameter');

        byobRequest.respond(1);

      });
    },
    type: 'bytes' });
  const reader = rs.getReader({ mode: 'byob' });
  return reader.read(new Uint8Array(1));

}, 'ReadableStreamBYOBRequest instances should have the correct list of properties');

test(() => {

  const methods = ['close', 'constructor', 'enqueue', 'error'];
  const accessors = ['byobRequest', 'desiredSize'];
  const properties = methods.concat(accessors).sort();

  let controller;
  new ReadableStream({
    start(c) {
      controller = c;
    },
    type: 'bytes'
  });
  const proto = Object.getPrototypeOf(controller);

  assert_array_equals(Object.getOwnPropertyNames(proto).sort(), properties);

  for (const m of methods) {
    const propDesc = Object.getOwnPropertyDescriptor(proto, m);
    assert_equals(propDesc.enumerable, false, 'method should be non-enumerable');
    assert_equals(propDesc.configurable, true, 'method should be configurable');
    assert_equals(propDesc.writable, true, 'method should be writable');
    assert_equals(typeof controller[m], 'function', 'should have be a method');
    const expectedName = m === 'constructor' ? 'ReadableByteStreamController' : m;
    assert_equals(controller[m].name, expectedName, 'method should have the correct name');
  }

  for (const a of accessors) {
    const propDesc = Object.getOwnPropertyDescriptor(proto, a);
    assert_equals(propDesc.enumerable, false, `${a} should be non-enumerable`);
    assert_equals(propDesc.configurable, true, `${a} should be configurable`);
    assert_not_equals(propDesc.get, undefined, `${a} should have a getter`);
    assert_equals(propDesc.set, undefined, `${a} should not have a setter`);
  }

  assert_equals(controller.close.length, 0, 'cancel has no parameters');
  assert_equals(controller.constructor.length, 0, 'constructor has no parameters');
  assert_equals(controller.enqueue.length, 1, 'enqueue has 1 parameter');
  assert_equals(controller.error.length, 1, 'releaseLock has 1 parameter');

}, 'ReadableByteStreamController instances should have the correct list of properties');

done();

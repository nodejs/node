'use strict';

if (self.importScripts) {
  self.importScripts('../resources/test-utils.js');
  self.importScripts('/resources/testharness.js');
}

let ReadableStreamBYOBReader;
let ReadableByteStreamController;

test(() => {

  // It's not exposed globally, but we test a few of its properties here.
  ReadableStreamBYOBReader = realRSBYOBReader().constructor;

  assert_equals(ReadableStreamBYOBReader.name, 'ReadableStreamBYOBReader', 'ReadableStreamBYOBReader should be set');

}, 'Can get the ReadableStreamBYOBReader constructor indirectly');

test(() => {

  // It's not exposed globally, but we test a few of its properties here.
  ReadableByteStreamController = realRBSController().constructor;

  assert_equals(ReadableByteStreamController.name, 'ReadableByteStreamController',
                'ReadableByteStreamController should be set');

}, 'Can get the ReadableByteStreamController constructor indirectly');

function fakeRS() {
  return Object.setPrototypeOf({
    cancel() { return Promise.resolve(); },
    getReader() { return realRSBYOBReader(); },
    pipeThrough(obj) { return obj.readable; },
    pipeTo() { return Promise.resolve(); },
    tee() { return [realRS(), realRS()]; }
  }, ReadableStream.prototype);
}

function realRS() {
  return new ReadableStream();
}

function fakeRSBYOBReader() {
  return Object.setPrototypeOf({
    get closed() { return Promise.resolve(); },
    cancel() { return Promise.resolve(); },
    read() { return Promise.resolve({ value: undefined, done: true }); },
    releaseLock() { return; }
  }, ReadableStreamBYOBReader.prototype);
}

function realRSBYOBReader() {
  return new ReadableStream({ type: 'bytes' }).getReader({ mode: 'byob' });
}

function fakeRBSController() {
  return Object.setPrototypeOf({
    close() { },
    enqueue() { },
    error() { }
  }, ReadableByteStreamController.prototype);
}

function realRBSController() {
  let controller;
  new ReadableStream({
    start(c) {
      controller = c;
    },
    type: 'bytes'
  });
  return controller;
}

test(() => {

  assert_throws(new TypeError(), () => new ReadableStreamBYOBReader(fakeRS()), 'constructor should throw');

}, 'ReadableStreamBYOBReader enforces a brand check on its argument');

promise_test(t => {

  return getterRejectsForAll(t, ReadableStreamBYOBReader.prototype, 'closed',
                             [fakeRSBYOBReader(), realRS(), realRBSController(), undefined, null]);

}, 'ReadableStreamBYOBReader.prototype.closed enforces a brand check');

promise_test(t => {

  return methodRejectsForAll(t, ReadableStreamBYOBReader.prototype, 'cancel',
                             [fakeRSBYOBReader(), realRS(), realRBSController(), undefined, null]);

}, 'ReadableStreamBYOBReader.prototype.cancel enforces a brand check');

promise_test(t => {

  return methodRejectsForAll(t, ReadableStreamBYOBReader.prototype, 'read',
                             [fakeRSBYOBReader(), realRS(), realRBSController(), undefined, null], [new Uint8Array(1)]);

}, 'ReadableStreamBYOBReader.prototype.read enforces a brand check');

test(() => {

  methodThrowsForAll(ReadableStreamBYOBReader.prototype, 'releaseLock',
                     [fakeRSBYOBReader(), realRS(), realRBSController(), undefined, null]);

}, 'ReadableStreamBYOBReader.prototype.releaseLock enforces a brand check');

test(() => {

  assert_throws(new TypeError(), () => new ReadableByteStreamController(fakeRS()),
                'Constructing a ReadableByteStreamController should throw');

}, 'ReadableByteStreamController enforces a brand check on its arguments');

test(() => {

  assert_throws(new TypeError(), () => new ReadableByteStreamController(realRS()),
                'Constructing a ReadableByteStreamController should throw');

}, 'ReadableByteStreamController can\'t be given a fully-constructed ReadableStream');

test(() => {

  getterThrowsForAll(ReadableByteStreamController.prototype, 'byobRequest',
                     [fakeRBSController(), realRS(), realRSBYOBReader(), undefined, null]);

}, 'ReadableByteStreamController.prototype.byobRequest enforces a brand check');

test(() => {

  methodThrowsForAll(ReadableByteStreamController.prototype, 'close',
                     [fakeRBSController(), realRS(), realRSBYOBReader(), undefined, null]);

}, 'ReadableByteStreamController.prototype.close enforces a brand check');

test(() => {

  methodThrowsForAll(ReadableByteStreamController.prototype, 'enqueue',
                     [fakeRBSController(), realRS(), realRSBYOBReader(), undefined, null], [new Uint8Array(1)]);

}, 'ReadableByteStreamController.prototype.enqueue enforces a brand check');

test(() => {

  methodThrowsForAll(ReadableByteStreamController.prototype, 'error',
                     [fakeRBSController(), realRS(), realRSBYOBReader(), undefined, null]);

}, 'ReadableByteStreamController.prototype.error enforces a brand check');

// ReadableStreamBYOBRequest can only be accessed asynchronously, so cram everything into one test.
promise_test(t => {

  let ReadableStreamBYOBRequest;
  const rs = new ReadableStream({
    pull(controller) {
      return t.step(() => {
        const byobRequest = controller.byobRequest;
        ReadableStreamBYOBRequest = byobRequest.constructor;
        brandChecks();
        byobRequest.respond(1);
      });
    },
    type: 'bytes'
  });
  const reader = rs.getReader({ mode: 'byob' });
  return reader.read(new Uint8Array(1));

  function fakeRSBYOBRequest() {
    return Object.setPrototypeOf({
      get view() {},
      respond() {},
      respondWithNewView() {}
    }, ReadableStreamBYOBRequest.prototype);
  }

  function brandChecks() {
    for (const badController of [fakeRBSController(), realRS(), realRSBYOBReader(), undefined, null]) {
      assert_throws(new TypeError(), () => new ReadableStreamBYOBRequest(badController, new Uint8Array(1)),
                    'ReadableStreamBYOBRequest constructor must throw for an invalid controller argument');
    }
    getterThrowsForAll(ReadableStreamBYOBRequest.prototype, 'view',
                       [fakeRSBYOBRequest(), realRS(), realRSBYOBReader(), realRBSController(), undefined, null]);
    methodThrowsForAll(ReadableStreamBYOBRequest.prototype, 'respond',
                       [fakeRSBYOBRequest(), realRS(), realRSBYOBReader(), realRBSController(), undefined, null], [1]);
    methodThrowsForAll(ReadableStreamBYOBRequest.prototype, 'respondWithNewView',
                       [fakeRSBYOBRequest(), realRS(), realRSBYOBReader(), realRBSController(), undefined, null],
                       [new Uint8Array(1)]);
  }

}, 'ReadableStreamBYOBRequest enforces brand checks');

done();

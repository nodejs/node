'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
  self.importScripts('../resources/test-utils.js');
}

const WritableStreamDefaultWriter = new WritableStream().getWriter().constructor;
const WriterProto = WritableStreamDefaultWriter.prototype;
const WritableStreamDefaultController = getWritableStreamDefaultControllerConstructor();

function getWritableStreamDefaultControllerConstructor() {
  return realWSDefaultController().constructor;
}

function fakeWS() {
  return Object.setPrototypeOf({
    get locked() { return false; },
    abort() { return Promise.resolve(); },
    getWriter() { return fakeWSDefaultWriter(); }
  }, WritableStream.prototype);
}

function realWS() {
  return new WritableStream();
}

function fakeWSDefaultWriter() {
  return Object.setPrototypeOf({
    get closed() { return Promise.resolve(); },
    get desiredSize() { return 1; },
    get ready() { return Promise.resolve(); },
    abort() { return Promise.resolve(); },
    close() { return Promise.resolve(); },
    write() { return Promise.resolve(); }
  }, WritableStreamDefaultWriter.prototype);
}

function realWSDefaultWriter() {
  const ws = new WritableStream();
  return ws.getWriter();
}

function fakeWSDefaultController() {
  return Object.setPrototypeOf({
    error() { return Promise.resolve(); }
  }, WritableStreamDefaultController.prototype);
}

function realWSDefaultController() {
  let controller;
  new WritableStream({
    start(c) {
      controller = c;
    }
  });
  return controller;
}

test(() => {
  getterThrowsForAll(WritableStream.prototype, 'locked',
                     [fakeWS(), realWSDefaultWriter(), realWSDefaultController(), undefined, null]);
}, 'WritableStream.prototype.locked enforces a brand check');

promise_test(t => {
  return methodRejectsForAll(t, WritableStream.prototype, 'abort',
                             [fakeWS(), realWSDefaultWriter(), realWSDefaultController(), undefined, null]);
}, 'WritableStream.prototype.abort enforces a brand check');

test(() => {
  methodThrowsForAll(WritableStream.prototype, 'getWriter',
                     [fakeWS(), realWSDefaultWriter(), realWSDefaultController(), undefined, null]);
}, 'WritableStream.prototype.getWriter enforces a brand check');

test(() => {
  assert_throws(new TypeError(), () => new WritableStreamDefaultWriter(fakeWS()), 'constructor should throw');
}, 'WritableStreamDefaultWriter constructor enforces a brand check');

test(() => {
  getterThrowsForAll(WriterProto, 'desiredSize',
                     [fakeWSDefaultWriter(), realWS(), realWSDefaultController(), undefined, null]);
}, 'WritableStreamDefaultWriter.prototype.desiredSize enforces a brand check');

promise_test(t => {
  return getterRejectsForAll(t, WriterProto, 'closed',
                             [fakeWSDefaultWriter(), realWS(), realWSDefaultController(), undefined, null]);
}, 'WritableStreamDefaultWriter.prototype.closed enforces a brand check');

promise_test(t => {
  return getterRejectsForAll(t, WriterProto, 'ready',
                             [fakeWSDefaultWriter(), realWS(), realWSDefaultController(), undefined, null]);
}, 'WritableStreamDefaultWriter.prototype.ready enforces a brand check');

promise_test(t => {
  return methodRejectsForAll(t, WriterProto, 'abort',
                             [fakeWSDefaultWriter(), realWS(), realWSDefaultController(), undefined, null]);
}, 'WritableStreamDefaultWriter.prototype.abort enforces a brand check');

promise_test(t => {
  return methodRejectsForAll(t, WriterProto, 'write',
                             [fakeWSDefaultWriter(), realWS(), realWSDefaultController(), undefined, null]);
}, 'WritableStreamDefaultWriter.prototype.write enforces a brand check');

promise_test(t => {
  return methodRejectsForAll(t, WriterProto, 'close',
                             [fakeWSDefaultWriter(), realWS(), realWSDefaultController(), undefined, null]);
}, 'WritableStreamDefaultWriter.prototype.close enforces a brand check');

test(() => {
  methodThrowsForAll(WritableStreamDefaultController.prototype, 'error',
                     [fakeWSDefaultController(), realWS(), realWSDefaultWriter(), undefined, null]);
}, 'WritableStreamDefaultController.prototype.error enforces a brand check');

done();

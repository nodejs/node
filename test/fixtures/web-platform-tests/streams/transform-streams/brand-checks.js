'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
  self.importScripts('../resources/test-utils.js');
}

const TransformStreamDefaultController = getTransformStreamDefaultControllerConstructor();

function getTransformStreamDefaultControllerConstructor() {
  return realTSDefaultController().constructor;
}

function fakeTS() {
  return Object.setPrototypeOf({
    get readable() { return new ReadableStream(); },
    get writable() { return new WritableStream(); }
  }, TransformStream.prototype);
}

function realTS() {
  return new TransformStream();
}

function fakeTSDefaultController() {
  return Object.setPrototypeOf({
    get desiredSize() { return 1; },
    enqueue() { },
    close() { },
    error() { }
  }, TransformStreamDefaultController.prototype);
}

function realTSDefaultController() {
  let controller;
  new TransformStream({
    start(c) {
      controller = c;
    }
  });
  return controller;
}

test(() => {
  getterThrowsForAll(TransformStream.prototype, 'readable',
                     [fakeTS(), realTSDefaultController(), undefined, null]);
}, 'TransformStream.prototype.readable enforces a brand check');

test(() => {
  getterThrowsForAll(TransformStream.prototype, 'writable',
                     [fakeTS(), realTSDefaultController(), undefined, null]);
}, 'TransformStream.prototype.writable enforces a brand check');

test(() => {
  constructorThrowsForAll(TransformStreamDefaultController,
                          [fakeTS(), realTS(), realTSDefaultController(), undefined, null]);
}, 'TransformStreamDefaultConstructor enforces a brand check and doesn\'t permit independent construction');

test(() => {
  getterThrowsForAll(TransformStreamDefaultController.prototype, 'desiredSize',
                     [fakeTSDefaultController(), realTS(), undefined, null]);
}, 'TransformStreamDefaultController.prototype.desiredSize enforces a brand check');

test(() => {
  methodThrowsForAll(TransformStreamDefaultController.prototype, 'enqueue',
                     [fakeTSDefaultController(), realTS(), undefined, null]);
}, 'TransformStreamDefaultController.prototype.enqueue enforces a brand check');

test(() => {
  methodThrowsForAll(TransformStreamDefaultController.prototype, 'terminate',
                     [fakeTSDefaultController(), realTS(), undefined, null]);
}, 'TransformStreamDefaultController.prototype.terminate enforces a brand check');

test(() => {
  methodThrowsForAll(TransformStreamDefaultController.prototype, 'error',
                     [fakeTSDefaultController(), realTS(), undefined, null]);
}, 'TransformStreamDefaultController.prototype.error enforces a brand check');

done();

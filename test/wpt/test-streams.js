'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('streams');

// Set Node.js flags required for the tests.
runner.setFlags(['--expose-internals']);

// Set a script that will be executed in the worker before running the tests.
runner.setInitScript(`
  let {
    ReadableStream,
    ReadableStreamDefaultReader,
    ReadableStreamBYOBReader,
    ReadableStreamBYOBRequest,
    ReadableByteStreamController,
    ReadableStreamDefaultController,
    TransformStream,
    TransformStreamDefaultController,
    WritableStream,
    WritableStreamDefaultWriter,
    WritableStreamDefaultController,
    ByteLengthQueuingStrategy,
    CountQueuingStrategy,
  } = require('stream/web');

  const { internalBinding } = require('internal/test/binding');
  const { DOMException } = internalBinding('messaging');
  global.DOMException = DOMException;

  Object.defineProperties(global, {
    ReadableStream: {
      value: ReadableStream,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    ReadableStreamDefaultReader: {
      value: ReadableStreamDefaultReader,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    ReadableStreamBYOBReader: {
      value: ReadableStreamBYOBReader,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    ReadableStreamBYOBRequest: {
      value: ReadableStreamBYOBRequest,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    ReadableByteStreamController: {
      value: ReadableByteStreamController,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    ReadableStreamDefaultController: {
      value: ReadableStreamDefaultController,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    TransformStream: {
      value: TransformStream,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    TransformStreamDefaultController: {
      value: TransformStreamDefaultController,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    WritableStream: {
      value: WritableStream,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    WritableStreamDefaultWriter: {
      value: WritableStreamDefaultWriter,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    WritableStreamDefaultController: {
      value: WritableStreamDefaultController,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    ByteLengthQueuingStrategy: {
      value: ByteLengthQueuingStrategy,
      configurable: true,
      writable: true,
      enumerable: false,
    },
    CountQueuingStrategy: {
      value: CountQueuingStrategy,
      configurable: true,
      writable: true,
      enumerable: false,
    },
  });

  // Simulate global postMessage for enqueue-with-detached-buffer.window.js
  function postMessage(value, origin, transferList) {
    const mc = new MessageChannel();
    mc.port1.postMessage(value, transferList);
    mc.port2.close();
  }

  // TODO(@jasnell): This is a bit of a hack to get the idl harness test
  // working. Later we should investigate a better approach.
  // See: https://github.com/nodejs/node/pull/39062#discussion_r659383373
  Object.defineProperties(global, {
    DedicatedWorkerGlobalScope: {
      get() {
        // Pretend that we're a DedicatedWorker, but *only* for the
        // IDL harness. For everything else, keep the JavaScript shell
        // environment.
        if (new Error().stack.includes('idlharness.js'))
          return global.constructor;
        else
          return function() {};
      }
    }
  });
`);

runner.runJsTests();

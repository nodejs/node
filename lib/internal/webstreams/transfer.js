'use strict';

const {
  PromiseResolve,
} = primordials;

const {
  kState,
  setPromiseHandled,
} = require('internal/webstreams/util');

const {
  DOMException,
} = internalBinding('errors');

const {
  ReadableStream,
  readableStreamDefaultControllerEnqueue,
  readableStreamDefaultControllerClose,
  readableStreamDefaultControllerError,
  readableStreamPipeTo,
} = require('internal/webstreams/readablestream');

const {
  WritableStream,
  writableStreamDefaultControllerErrorIfNeeded,
} = require('internal/webstreams/writablestream');

const {
  createDeferredPromise,
} = require('internal/util');

const assert = require('internal/assert');

class CrossRealmTransformReadableSource {
  constructor(port) {
    this[kState] = {
      port,
      controller: undefined,
    };

    port.onmessage = ({ data }) => {
      const {
        controller,
      } = this[kState];
      const {
        type,
        value,
      } = data;
      switch (type) {
        case 'chunk':
          readableStreamDefaultControllerEnqueue(
            controller,
            value);
          break;
        case 'close':
          readableStreamDefaultControllerClose(controller);
          port.close();
          break;
        case 'error':
          readableStreamDefaultControllerError(controller, value);
          port.close();
          break;
      }
    };

    port.onmessageerror = () => {
      const error = new DOMException(
        'Internal transferred ReadableStream error',
        'DataCloneError');
      port.postMessage({ type: 'error', value: error });
      readableStreamDefaultControllerError(
        this[kState].controller,
        error);
      port.close();
    };
  }

  start(controller) {
    this[kState].controller = controller;
  }

  async pull() {
    this[kState].port.postMessage({ type: 'pull' });
  }

  async cancel(reason) {
    try {
      this[kState].port.postMessage({ type: 'error', value: reason });
    } catch (error) {
      this[kState].port.postMessage({ type: 'error', value: error });
      throw error;
    } finally {
      this[kState].port.close();
    }
  }
}

class CrossRealmTransformWritableSink {
  constructor(port) {
    this[kState] = {
      port,
      controller: undefined,
      backpressurePromise: createDeferredPromise(),
    };

    port.onmessage = ({ data }) => {
      assert(typeof data === 'object');
      const {
        type,
        value
      } = { ...data };
      assert(typeof type === 'string');
      switch (type) {
        case 'pull':
          if (this[kState].backpressurePromise !== undefined)
            this[kState].backpressurePromise.resolve?.();
          this[kState].backpressurePromise = undefined;
          break;
        case 'error':
          writableStreamDefaultControllerErrorIfNeeded(
            this[kState].controller,
            value);
          if (this[kState].backpressurePromise !== undefined)
            this[kState].backpressurePromise.resolve?.();
          this[kState].backpressurePromise = undefined;
          break;
      }
    };
    port.onmessageerror = () => {
      const error = new DOMException(
        'Internal transferred ReadableStream error',
        'DataCloneError');
      port.postMessage({ type: 'error', value: error });
      writableStreamDefaultControllerErrorIfNeeded(
        this[kState].controller,
        error);
      port.close();
    };

  }

  start(controller) {
    this[kState].controller = controller;
  }

  async write(chunk) {
    if (this[kState].backpressurePromise === undefined) {
      this[kState].backpressurePromise = {
        promise: PromiseResolve(),
        resolve: undefined,
        reject: undefined,
      };
    }
    await this[kState].backpressurePromise.promise;
    this[kState].backpressurePromise = createDeferredPromise();
    try {
      this[kState].port.postMessage({ type: 'chunk', value: chunk });
    } catch (error) {
      this[kState].port.postMessage({ type: 'error', value: error });
      this[kState].port.close();
      throw error;
    }
  }

  close() {
    this[kState].port.postMessage({ type: 'close' });
    this[kState].port.close();
  }

  abort(reason) {
    try {
      this[kState].port.postMessage({ type: 'error', value: reason });
    } catch (error) {
      this[kState].port.postMessage({ type: 'error', value: error });
      throw error;
    } finally {
      this[kState].port.close();
    }
  }
}

function newCrossRealmReadableStream(writable, port) {
  const readable =
    new ReadableStream(
      new CrossRealmTransformReadableSource(port));

  const promise =
    readableStreamPipeTo(readable, writable, false, false, false);

  setPromiseHandled(promise);

  return {
    readable,
    promise,
  };
}

function newCrossRealmWritableSink(readable, port) {
  const writable =
    new WritableStream(
      new CrossRealmTransformWritableSink(port));

  const promise = readableStreamPipeTo(readable, writable, false, false, false);
  setPromiseHandled(promise);
  return {
    writable,
    promise,
  };
}

module.exports = {
  newCrossRealmReadableStream,
  newCrossRealmWritableSink,
  CrossRealmTransformWritableSink,
  CrossRealmTransformReadableSource,
};

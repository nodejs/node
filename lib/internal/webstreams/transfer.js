'use strict';

const {
  PromiseResolve,
  PromiseWithResolvers,
} = primordials;

const {
  kState,
  setPromiseHandled,
} = require('internal/webstreams/util');

const {
  DOMException,
} = internalBinding('messaging');

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

const assert = require('internal/assert');

class CrossRealmTransformReadableSource {
  constructor(port, unref) {
    this[kState] = {
      port,
      controller: undefined,
      unref,
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

    port.unref();
  }

  start(controller) {
    this[kState].controller = controller;
  }

  async pull() {
    if (this[kState].unref) {
      this[kState].unref = false;
      this[kState].port.ref();
    }
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
  constructor(port, unref) {
    this[kState] = {
      port,
      controller: undefined,
      backpressurePromise: PromiseWithResolvers(),
      unref,
    };

    port.onmessage = ({ data }) => {
      assert(typeof data === 'object');
      const {
        type,
        value,
      } = { ...data };
      assert(typeof type === 'string');
      switch (type) {
        case 'pull':
          if (this[kState].backpressurePromise !== undefined)
            this[kState].backpressurePromise.resolve();
          this[kState].backpressurePromise = undefined;
          break;
        case 'error':
          writableStreamDefaultControllerErrorIfNeeded(
            this[kState].controller,
            value);
          if (this[kState].backpressurePromise !== undefined)
            this[kState].backpressurePromise.resolve();
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

    port.unref();
  }

  start(controller) {
    this[kState].controller = controller;
  }

  async write(chunk) {
    if (this[kState].unref) {
      this[kState].unref = false;
      this[kState].port.ref();
    }
    if (this[kState].backpressurePromise === undefined) {
      this[kState].backpressurePromise = {
        promise: PromiseResolve(),
        resolve: undefined,
        reject: undefined,
      };
    }
    await this[kState].backpressurePromise.promise;
    this[kState].backpressurePromise = PromiseWithResolvers();
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
  // MessagePort should always be unref.
  // There is a problem with the process not terminating.
  // https://github.com/nodejs/node/issues/44985
  const readable = new ReadableStream(new CrossRealmTransformReadableSource(port, false));

  const promise = readableStreamPipeTo(readable, writable, false, false, false);

  setPromiseHandled(promise);

  return {
    readable,
    promise,
  };
}

function newCrossRealmWritableSink(readable, port) {
  // MessagePort should always be unref.
  // There is a problem with the process not terminating.
  // https://github.com/nodejs/node/issues/44985
  const writable = new WritableStream(new CrossRealmTransformWritableSink(port, false));

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

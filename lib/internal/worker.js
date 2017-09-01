'use strict';

const Buffer = require('buffer').Buffer;
const EventEmitter = require('events');
const assert = require('assert');
const path = require('path');
const util = require('util');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_WORKER_NEED_ABSOLUTE_PATH,
  ERR_WORKER_UNSERIALIZABLE_ERROR
} = require('internal/errors').codes;

const { internalBinding } = require('internal/bootstrap/loaders');
const { MessagePort, MessageChannel } = internalBinding('messaging');
const { handle_onclose } = internalBinding('symbols');
const { clearAsyncIdStack } = require('internal/async_hooks');

util.inherits(MessagePort, EventEmitter);

const {
  Worker: WorkerImpl,
  getEnvMessagePort,
  threadId
} = internalBinding('worker');

const isMainThread = threadId === 0;

const kOnMessageListener = Symbol('kOnMessageListener');
const kHandle = Symbol('kHandle');
const kPort = Symbol('kPort');
const kPublicPort = Symbol('kPublicPort');
const kDispose = Symbol('kDispose');
const kOnExit = Symbol('kOnExit');
const kOnMessage = Symbol('kOnMessage');
const kOnCouldNotSerializeErr = Symbol('kOnCouldNotSerializeErr');
const kOnErrorMessage = Symbol('kOnErrorMessage');

const debug = util.debuglog('worker');

// A communication channel consisting of a handle (that wraps around an
// uv_async_t) which can receive information from other threads and emits
// .onmessage events, and a function used for sending data to a MessagePort
// in some other thread.
MessagePort.prototype[kOnMessageListener] = function onmessage(payload) {
  debug(`[${threadId}] received message`, payload);
  // Emit the deserialized object to userland.
  this.emit('message', payload);
};

// This is for compatibility with the Web's MessagePort API. It makes sense to
// provide it as an `EventEmitter` in Node.js, but if somebody overrides
// `onmessage`, we'll switch over to the Web API model.
Object.defineProperty(MessagePort.prototype, 'onmessage', {
  enumerable: true,
  configurable: true,
  get() {
    return this[kOnMessageListener];
  },
  set(value) {
    this[kOnMessageListener] = value;
    if (typeof value === 'function') {
      this.ref();
      this.start();
    } else {
      this.unref();
      this.stop();
    }
  }
});

// This is called from inside the `MessagePort` constructor.
function oninit() {
  setupPortReferencing(this, this, 'message');
}

Object.defineProperty(MessagePort.prototype, 'oninit', {
  enumerable: true,
  writable: false,
  value: oninit
});

// This is called after the underlying `uv_async_t` has been closed.
function onclose() {
  if (typeof this.onclose === 'function') {
    // Not part of the Web standard yet, but there aren't many reasonable
    // alternatives in a non-EventEmitter usage setting.
    // Refs: https://github.com/whatwg/html/issues/1766
    this.onclose();
  }
  this.emit('close');
}

Object.defineProperty(MessagePort.prototype, handle_onclose, {
  enumerable: false,
  writable: false,
  value: onclose
});

const originalClose = MessagePort.prototype.close;
MessagePort.prototype.close = function(cb) {
  if (typeof cb === 'function')
    this.once('close', cb);
  originalClose.call(this);
};

const drainMessagePort = MessagePort.prototype.drain;
delete MessagePort.prototype.drain;

function setupPortReferencing(port, eventEmitter, eventName) {
  // Keep track of whether there are any workerMessage listeners:
  // If there are some, ref() the channel so it keeps the event loop alive.
  // If there are none or all are removed, unref() the channel so the worker
  // can shutdown gracefully.
  port.unref();
  eventEmitter.on('newListener', (name) => {
    if (name === eventName && eventEmitter.listenerCount(eventName) === 0) {
      port.ref();
      port.start();
    }
  });
  eventEmitter.on('removeListener', (name) => {
    if (name === eventName && eventEmitter.listenerCount(eventName) === 0) {
      port.stop();
      port.unref();
    }
  });
}


class Worker extends EventEmitter {
  constructor(filename, options = {}) {
    super();
    debug(`[${threadId}] create new worker`, filename, options);
    if (typeof filename !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('filename', 'string', filename);
    }

    if (!options.eval && !path.isAbsolute(filename)) {
      throw new ERR_WORKER_NEED_ABSOLUTE_PATH(filename);
    }

    // Set up the C++ handle for the worker, as well as some internal wiring.
    this[kHandle] = new WorkerImpl();
    this[kHandle].onexit = (code) => this[kOnExit](code);
    this[kPort] = this[kHandle].messagePort;
    this[kPort].on('message', (data) => this[kOnMessage](data));
    this[kPort].start();
    this[kPort].unref();
    debug(`[${threadId}] created Worker with ID ${this.threadId}`);

    const { port1, port2 } = new MessageChannel();
    this[kPublicPort] = port1;
    this[kPublicPort].on('message', (message) => this.emit('message', message));
    setupPortReferencing(this[kPublicPort], this, 'message');
    this[kPort].postMessage({
      type: 'loadScript',
      filename,
      doEval: !!options.eval,
      workerData: options.workerData,
      publicPort: port2
    }, [port2]);
    // Actually start the new thread now that everything is in place.
    this[kHandle].startThread();
  }

  [kOnExit](code) {
    debug(`[${threadId}] hears end event for Worker ${this.threadId}`);
    drainMessagePort.call(this[kPublicPort]);
    this[kDispose]();
    this.emit('exit', code);
    this.removeAllListeners();
  }

  [kOnCouldNotSerializeErr]() {
    this.emit('error', new ERR_WORKER_UNSERIALIZABLE_ERROR());
  }

  [kOnErrorMessage](serialized) {
    // This is what is called for uncaught exceptions.
    const error = deserializeError(serialized);
    this.emit('error', error);
  }

  [kOnMessage](message) {
    switch (message.type) {
      case 'upAndRunning':
        return this.emit('online');
      case 'couldNotSerializeError':
        return this[kOnCouldNotSerializeErr]();
      case 'errorMessage':
        return this[kOnErrorMessage](message.error);
    }

    assert.fail(`Unknown worker message type ${message.type}`);
  }

  [kDispose]() {
    this[kHandle].onexit = null;
    this[kHandle] = null;
    this[kPort] = null;
    this[kPublicPort] = null;
  }

  postMessage(...args) {
    this[kPublicPort].postMessage(...args);
  }

  terminate(callback) {
    if (this[kHandle] === null) return;

    debug(`[${threadId}] terminates Worker with ID ${this.threadId}`);

    if (typeof callback !== 'undefined')
      this.once('exit', (exitCode) => callback(null, exitCode));

    this[kHandle].stopThread();
  }

  ref() {
    if (this[kHandle] === null) return;

    this[kHandle].ref();
    this[kPublicPort].ref();
  }

  unref() {
    if (this[kHandle] === null) return;

    this[kHandle].unref();
    this[kPublicPort].unref();
  }

  get threadId() {
    if (this[kHandle] === null) return -1;

    return this[kHandle].threadId;
  }
}

let originalFatalException;

function setupChild(evalScript) {
  // Called during bootstrap to set up worker script execution.
  debug(`[${threadId}] is setting up worker child environment`);
  const port = getEnvMessagePort();

  const publicWorker = require('worker');

  port.on('message', (message) => {
    if (message.type === 'loadScript') {
      const { filename, doEval, workerData, publicPort } = message;
      publicWorker.parentPort = publicPort;
      setupPortReferencing(publicPort, publicPort, 'message');
      publicWorker.workerData = workerData;
      debug(`[${threadId}] starts worker script ${filename} ` +
            `(eval = ${eval}) at cwd = ${process.cwd()}`);
      port.unref();
      port.postMessage({ type: 'upAndRunning' });
      if (doEval) {
        evalScript('[worker eval]', filename);
      } else {
        process.argv[1] = filename; // script filename
        require('module').runMain();
      }
      return;
    }

    assert.fail(`Unknown worker message type ${message.type}`);
  });

  port.start();

  originalFatalException = process._fatalException;
  process._fatalException = fatalException;

  function fatalException(error) {
    debug(`[${threadId}] gets fatal exception`);
    let caught = false;
    try {
      caught = originalFatalException.call(this, error);
    } catch (e) {
      error = e;
    }
    debug(`[${threadId}] fatal exception caught = ${caught}`);

    if (!caught) {
      let serialized;
      try {
        serialized = serializeError(error);
      } catch {}
      debug(`[${threadId}] fatal exception serialized = ${!!serialized}`);
      if (serialized)
        port.postMessage({ type: 'errorMessage', error: serialized });
      else
        port.postMessage({ type: 'couldNotSerializeError' });
      clearAsyncIdStack();
    }
  }
}

// TODO(addaleax): These can be improved a lot.
function serializeError(error) {
  return Buffer.from(util.inspect(error), 'utf8');
}

function deserializeError(error) {
  return Buffer.from(error.buffer,
                     error.byteOffset,
                     error.byteLength).toString('utf8');
}

module.exports = {
  MessagePort,
  MessageChannel,
  threadId,
  Worker,
  setupChild,
  isMainThread
};

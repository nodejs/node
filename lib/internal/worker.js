'use strict';

const EventEmitter = require('events');
const assert = require('assert');
const path = require('path');
const util = require('util');
const { Readable, Writable } = require('stream');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_WORKER_PATH,
  ERR_WORKER_UNSERIALIZABLE_ERROR,
  ERR_WORKER_UNSUPPORTED_EXTENSION,
} = require('internal/errors').codes;

const { internalBinding } = require('internal/bootstrap/loaders');
const { MessagePort, MessageChannel } = internalBinding('messaging');
const { handle_onclose } = internalBinding('symbols');
const locks = internalBinding('locks');
const { clearAsyncIdStack } = require('internal/async_hooks');
const { serializeError, deserializeError } = require('internal/error-serdes');
const DOMException = require('internal/domexception');
const nextTickUtil = require('internal/process/next_tick');

util.inherits(MessagePort, EventEmitter);

const {
  Worker: WorkerImpl,
  getEnvMessagePort,
  threadId
} = internalBinding('worker');

const isMainThread = threadId === 0;

const kOnMessageListener = Symbol('kOnMessageListener');
const kHandle = Symbol('kHandle');
const kName = Symbol('kName');
const kPort = Symbol('kPort');
const kPublicPort = Symbol('kPublicPort');
const kDispose = Symbol('kDispose');
const kOnExit = Symbol('kOnExit');
const kOnMessage = Symbol('kOnMessage');
const kOnCouldNotSerializeErr = Symbol('kOnCouldNotSerializeErr');
const kOnErrorMessage = Symbol('kOnErrorMessage');
const kParentSideStdio = Symbol('kParentSideStdio');
const kWritableCallbacks = Symbol('kWritableCallbacks');
const kStdioWantsMoreDataCallback = Symbol('kStdioWantsMoreDataCallback');
const kStartedReading = Symbol('kStartedReading');
const kWaitingStreams = Symbol('kWaitingStreams');
const kIncrementsPortRef = Symbol('kIncrementsPortRef');
const kMode = Symbol('mode');

const debug = util.debuglog('worker');

const messageTypes = {
  UP_AND_RUNNING: 'upAndRunning',
  COULD_NOT_SERIALIZE_ERROR: 'couldNotSerializeError',
  ERROR_MESSAGE: 'errorMessage',
  STDIO_PAYLOAD: 'stdioPayload',
  STDIO_WANTS_MORE_DATA: 'stdioWantsMoreData',
  LOAD_SCRIPT: 'loadScript'
};

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

Object.defineProperty(MessagePort.prototype, util.inspect.custom, {
  enumerable: false,
  writable: false,
  value: function inspect() {  // eslint-disable-line func-name-matching
    let ref;
    try {
      // This may throw when `this` does not refer to a native object,
      // e.g. when accessing the prototype directly.
      ref = this.hasRef();
    } catch { return this; }
    return Object.assign(Object.create(MessagePort.prototype),
                         ref === undefined ? {
                           active: false,
                         } : {
                           active: true,
                           refed: ref
                         },
                         this);
  }
});

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


class ReadableWorkerStdio extends Readable {
  constructor(port, name) {
    super();
    this[kPort] = port;
    this[kName] = name;
    this[kIncrementsPortRef] = true;
    this[kStartedReading] = false;
    this.on('end', () => {
      if (this[kIncrementsPortRef] && --this[kPort][kWaitingStreams] === 0)
        this[kPort].unref();
    });
  }

  _read() {
    if (!this[kStartedReading] && this[kIncrementsPortRef]) {
      this[kStartedReading] = true;
      if (this[kPort][kWaitingStreams]++ === 0)
        this[kPort].ref();
    }

    this[kPort].postMessage({
      type: messageTypes.STDIO_WANTS_MORE_DATA,
      stream: this[kName]
    });
  }
}

class WritableWorkerStdio extends Writable {
  constructor(port, name) {
    super({ decodeStrings: false });
    this[kPort] = port;
    this[kName] = name;
    this[kWritableCallbacks] = [];
  }

  _write(chunk, encoding, cb) {
    this[kPort].postMessage({
      type: messageTypes.STDIO_PAYLOAD,
      stream: this[kName],
      chunk,
      encoding
    });
    this[kWritableCallbacks].push(cb);
    if (this[kPort][kWaitingStreams]++ === 0)
      this[kPort].ref();
  }

  _final(cb) {
    this[kPort].postMessage({
      type: messageTypes.STDIO_PAYLOAD,
      stream: this[kName],
      chunk: null
    });
    cb();
  }

  [kStdioWantsMoreDataCallback]() {
    const cbs = this[kWritableCallbacks];
    this[kWritableCallbacks] = [];
    for (const cb of cbs)
      cb();
    if ((this[kPort][kWaitingStreams] -= cbs.length) === 0)
      this[kPort].unref();
  }
}

class Worker extends EventEmitter {
  constructor(filename, options = {}) {
    super();
    debug(`[${threadId}] create new worker`, filename, options);
    if (typeof filename !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('filename', 'string', filename);
    }

    if (!options.eval) {
      if (!path.isAbsolute(filename) &&
          !filename.startsWith('./') &&
          !filename.startsWith('../') &&
          !filename.startsWith('.' + path.sep) &&
          !filename.startsWith('..' + path.sep)) {
        throw new ERR_WORKER_PATH(filename);
      }
      filename = path.resolve(filename);

      const ext = path.extname(filename);
      if (ext !== '.js' && ext !== '.mjs') {
        throw new ERR_WORKER_UNSUPPORTED_EXTENSION(ext);
      }
    }

    // Set up the C++ handle for the worker, as well as some internal wiring.
    this[kHandle] = new WorkerImpl();
    this[kHandle].onexit = (code) => this[kOnExit](code);
    this[kPort] = this[kHandle].messagePort;
    this[kPort].on('message', (data) => this[kOnMessage](data));
    this[kPort].start();
    this[kPort].unref();
    this[kPort][kWaitingStreams] = 0;
    debug(`[${threadId}] created Worker with ID ${this.threadId}`);

    let stdin = null;
    if (options.stdin)
      stdin = new WritableWorkerStdio(this[kPort], 'stdin');
    const stdout = new ReadableWorkerStdio(this[kPort], 'stdout');
    if (!options.stdout) {
      stdout[kIncrementsPortRef] = false;
      pipeWithoutWarning(stdout, process.stdout);
    }
    const stderr = new ReadableWorkerStdio(this[kPort], 'stderr');
    if (!options.stderr) {
      stderr[kIncrementsPortRef] = false;
      pipeWithoutWarning(stderr, process.stderr);
    }

    this[kParentSideStdio] = { stdin, stdout, stderr };

    const { port1, port2 } = new MessageChannel();
    this[kPublicPort] = port1;
    this[kPublicPort].on('message', (message) => this.emit('message', message));
    setupPortReferencing(this[kPublicPort], this, 'message');
    this[kPort].postMessage({
      type: messageTypes.LOAD_SCRIPT,
      filename,
      doEval: !!options.eval,
      workerData: options.workerData,
      publicPort: port2,
      hasStdin: !!options.stdin
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
      case messageTypes.UP_AND_RUNNING:
        return this.emit('online');
      case messageTypes.COULD_NOT_SERIALIZE_ERROR:
        return this[kOnCouldNotSerializeErr]();
      case messageTypes.ERROR_MESSAGE:
        return this[kOnErrorMessage](message.error);
      case messageTypes.STDIO_PAYLOAD:
      {
        const { stream, chunk, encoding } = message;
        return this[kParentSideStdio][stream].push(chunk, encoding);
      }
      case messageTypes.STDIO_WANTS_MORE_DATA:
      {
        const { stream } = message;
        return this[kParentSideStdio][stream][kStdioWantsMoreDataCallback]();
      }
    }

    assert.fail(`Unknown worker message type ${message.type}`);
  }

  [kDispose]() {
    this[kHandle].onexit = null;
    this[kHandle] = null;
    this[kPort] = null;
    this[kPublicPort] = null;

    const { stdout, stderr } = this[kParentSideStdio];
    this[kParentSideStdio] = null;

    if (!stdout._readableState.ended) {
      debug(`[${threadId}] explicitly closes stdout for ${this.threadId}`);
      stdout.push(null);
    }
    if (!stderr._readableState.ended) {
      debug(`[${threadId}] explicitly closes stderr for ${this.threadId}`);
      stderr.push(null);
    }
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

  get stdin() {
    return this[kParentSideStdio].stdin;
  }

  get stdout() {
    return this[kParentSideStdio].stdout;
  }

  get stderr() {
    return this[kParentSideStdio].stderr;
  }
}

const workerStdio = {};
if (!isMainThread) {
  const port = getEnvMessagePort();
  port[kWaitingStreams] = 0;
  workerStdio.stdin = new ReadableWorkerStdio(port, 'stdin');
  workerStdio.stdout = new WritableWorkerStdio(port, 'stdout');
  workerStdio.stderr = new WritableWorkerStdio(port, 'stderr');
}

let originalFatalException;

function setupChild(evalScript) {
  // Called during bootstrap to set up worker script execution.
  debug(`[${threadId}] is setting up worker child environment`);
  const port = getEnvMessagePort();

  const publicWorker = require('worker_threads');

  port.on('message', (message) => {
    if (message.type === messageTypes.LOAD_SCRIPT) {
      const { filename, doEval, workerData, publicPort, hasStdin } = message;
      publicWorker.parentPort = publicPort;
      publicWorker.workerData = workerData;

      if (!hasStdin)
        workerStdio.stdin.push(null);

      debug(`[${threadId}] starts worker script ${filename} ` +
            `(eval = ${eval}) at cwd = ${process.cwd()}`);
      port.unref();
      port.postMessage({ type: messageTypes.UP_AND_RUNNING });
      if (doEval) {
        evalScript('[worker eval]', filename);
      } else {
        process.argv[1] = filename; // script filename
        require('module').runMain();
      }
      return;
    } else if (message.type === messageTypes.STDIO_PAYLOAD) {
      const { stream, chunk, encoding } = message;
      workerStdio[stream].push(chunk, encoding);
      return;
    } else if (message.type === messageTypes.STDIO_WANTS_MORE_DATA) {
      const { stream } = message;
      workerStdio[stream][kStdioWantsMoreDataCallback]();
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
        port.postMessage({
          type: messageTypes.ERROR_MESSAGE,
          error: serialized
        });
      else
        port.postMessage({ type: messageTypes.COULD_NOT_SERIALIZE_ERROR });
      clearAsyncIdStack();

      process.exit();
    }
  }
}

function pipeWithoutWarning(source, dest) {
  const sourceMaxListeners = source._maxListeners;
  const destMaxListeners = dest._maxListeners;
  source.setMaxListeners(Infinity);
  dest.setMaxListeners(Infinity);

  source.pipe(dest);

  source._maxListeners = sourceMaxListeners;
  dest._maxListeners = destMaxListeners;
}

// https://wicg.github.io/web-locks/#api-lock
class Lock {
  constructor() {
    // eslint-disable-next-line no-restricted-syntax
    throw new TypeError('Illegal constructor');
  }

  get name() {
    return this[kName];
  }

  get mode() {
    return this[kMode];
  }
}

Object.defineProperties(Lock.prototype, {
  name: { enumerable: true },
  mode: { enumerable: true },
  [Symbol.toStringTag]: {
    value: 'Lock',
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

// https://wicg.github.io/web-locks/#api-lock-manager
class LockManager {
  constructor() {
    // eslint-disable-next-line no-restricted-syntax
    throw new TypeError('Illegal constructor');
  }

  // https://wicg.github.io/web-locks/#api-lock-manager-request
  request(name, options, callback) {
    if (callback === undefined) {
      callback = options;
      options = undefined;
    }

    // Let promise be a new promise.
    let reject;
    let resolve;
    const promise = new Promise((res, rej) => {
      resolve = res;
      reject = rej;
    });

    // If options was not passed, then let options be a new LockOptions
    // dictionary with default members.
    if (options === undefined) {
      options = {
        mode: 'exclusive',
        ifAvailable: false,
        steal: false,
      };
    }

    if (name.startsWith('-')) {
      // if name starts with U+002D HYPHEN-MINUS (-), then reject promise with a
      // "NotSupportedError" DOMException.
      reject(new DOMException('NotSupportedError'));
    } else if (options.ifAvailable === true && options.steal === true) {
      // Otherwise, if both options' steal dictionary member and option's
      // ifAvailable dictionary member are true, then reject promise with a
      // "NotSupportedError" DOMException.
      reject(new DOMException('NotSupportedError'));
    } else if (options.steal === true && options.mode !== 'exclusive') {
      // Otherwise, if options' steal dictionary member is true and option's
      // mode dictionary member is not "exclusive", then reject promise with a
      // "NotSupportedError" DOMException.
      reject(new DOMException('NotSupportedError'));
    } else {
      // Otherwise, run these steps:

      // Let request be the result of running the steps to request a lock with
      // promise, the current agent, environment's id, origin, callback, name,
      // options' mode dictionary member, options' ifAvailable dictionary
      // member, and option's steal dictionary member.
      nextTickUtil.queueMicrotask(() => {
        locks.request(
          promise,
          (name, mode, waitingPromise, release) => {
            const lock = Object.create(Lock.prototype, {
              [kName]: {
                value: name,
                writable: false,
                enumerable: false,
                configurable: false,
              },
              [kMode]: {
                value: mode === 0 ? 'shared' : 'exclusive',
                writable: false,
                enumerable: false,
                configurable: false,
              },
            });

            // When lock lock's waiting promise settles (fulfills or rejects),
            // enqueue the following steps on the lock task queue:
            waitingPromise
              .finally(() => undefined)
              .then(() => {
                // Release the lock lock.
                release();

                // Resolve lock's released promise with lock's waiting promise.
                resolve(waitingPromise);
              });

            return callback(lock);
          },
          name,
          options.mode === 'shared' ? 0 : 1,
          options.ifAvailable || false,
          options.steal || false);
      });
    }

    // Return promise.
    return promise;
  }

  // https://wicg.github.io/web-locks/#api-lock-manager-query
  query() {
    return new Promise((resolve) => {
      nextTickUtil.queueMicrotask(() => {
        const snapshot = locks.snapshot();
        resolve(snapshot);
      });
    });
  }
}

Object.defineProperties(LockManager.prototype, {
  request: { enumerable: true },
  query: { enumerable: true },
  [Symbol.toStringTag]: {
    value: 'LockManager',
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

module.exports = {
  MessagePort,
  MessageChannel,
  threadId,
  Worker,
  setupChild,
  isMainThread,
  workerStdio,
  LockManager,
};

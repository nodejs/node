'use strict';

const {
  handle_onclose: handleOnCloseSymbol,
  oninit: onInitSymbol
} = internalBinding('symbols');
const {
  MessagePort,
  MessageChannel
} = internalBinding('messaging');
const { threadId } = internalBinding('worker');

const { Readable, Writable } = require('stream');
const EventEmitter = require('events');
const util = require('util');
const debug = util.debuglog('worker');

const kIncrementsPortRef = Symbol('kIncrementsPortRef');
const kName = Symbol('kName');
const kOnMessageListener = Symbol('kOnMessageListener');
const kPort = Symbol('kPort');
const kWaitingStreams = Symbol('kWaitingStreams');
const kWritableCallbacks = Symbol('kWritableCallbacks');
const kStartedReading = Symbol('kStartedReading');
const kStdioWantsMoreDataCallback = Symbol('kStdioWantsMoreDataCallback');

const messageTypes = {
  UP_AND_RUNNING: 'upAndRunning',
  COULD_NOT_SERIALIZE_ERROR: 'couldNotSerializeError',
  ERROR_MESSAGE: 'errorMessage',
  STDIO_PAYLOAD: 'stdioPayload',
  STDIO_WANTS_MORE_DATA: 'stdioWantsMoreData',
  LOAD_SCRIPT: 'loadScript'
};

// Original drain from C++
const originalDrain = MessagePort.prototype.drain;

function drainMessagePort(port) {
  return originalDrain.call(port);
}

// We have to mess with the MessagePort prototype a bit, so that a) we can make
// it inherit from EventEmitter, even though it is a C++ class, and b) we do
// not provide methods that are not present in the Browser and not documented
// on our side (e.g. hasRef).
// Save a copy of the original set of methods as a shallow clone.
const MessagePortPrototype = Object.create(
  Object.getPrototypeOf(MessagePort.prototype),
  Object.getOwnPropertyDescriptors(MessagePort.prototype));
// Set up the new inheritance chain.
Object.setPrototypeOf(MessagePort, EventEmitter);
Object.setPrototypeOf(MessagePort.prototype, EventEmitter.prototype);
// Finally, purge methods we don't want to be public.
delete MessagePort.prototype.stop;
delete MessagePort.prototype.drain;
MessagePort.prototype.ref = MessagePortPrototype.ref;
MessagePort.prototype.unref = MessagePortPrototype.unref;

// A communication channel consisting of a handle (that wraps around an
// uv_async_t) which can receive information from other threads and emits
// .onmessage events, and a function used for sending data to a MessagePort
// in some other thread.
MessagePort.prototype[kOnMessageListener] = function onmessage(payload) {
  if (payload.type !== messageTypes.STDIO_WANTS_MORE_DATA)
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
      MessagePortPrototype.start.call(this);
    } else {
      this.unref();
      MessagePortPrototype.stop.call(this);
    }
  }
});

// This is called from inside the `MessagePort` constructor.
function oninit() {
  setupPortReferencing(this, this, 'message');
}

Object.defineProperty(MessagePort.prototype, onInitSymbol, {
  enumerable: true,
  writable: false,
  value: oninit
});

// This is called after the underlying `uv_async_t` has been closed.
function onclose() {
  this.emit('close');
}

Object.defineProperty(MessagePort.prototype, handleOnCloseSymbol, {
  enumerable: false,
  writable: false,
  value: onclose
});

MessagePort.prototype.close = function(cb) {
  if (typeof cb === 'function')
    this.once('close', cb);
  MessagePortPrototype.close.call(this);
};

Object.defineProperty(MessagePort.prototype, util.inspect.custom, {
  enumerable: false,
  writable: false,
  value: function inspect() {  // eslint-disable-line func-name-matching
    let ref;
    try {
      // This may throw when `this` does not refer to a native object,
      // e.g. when accessing the prototype directly.
      ref = MessagePortPrototype.hasRef.call(this);
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
      MessagePortPrototype.start.call(port);
    }
  });
  eventEmitter.on('removeListener', (name) => {
    if (name === eventName && eventEmitter.listenerCount(eventName) === 0) {
      MessagePortPrototype.stop.call(port);
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

module.exports = {
  drainMessagePort,
  messageTypes,
  kPort,
  kIncrementsPortRef,
  kWaitingStreams,
  kStdioWantsMoreDataCallback,
  MessagePort,
  MessageChannel,
  setupPortReferencing,
  ReadableWorkerStdio,
  WritableWorkerStdio
};

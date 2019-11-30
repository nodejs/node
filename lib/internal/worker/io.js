'use strict';

const {
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptors,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  Symbol,
} = primordials;

const {
  handle_onclose: handleOnCloseSymbol,
  oninit: onInitSymbol,
  no_message_symbol: noMessageSymbol
} = internalBinding('symbols');
const {
  MessagePort,
  MessageChannel,
  drainMessagePort,
  moveMessagePortToContext,
  receiveMessageOnPort: receiveMessageOnPort_,
  stopMessagePort
} = internalBinding('messaging');
const {
  threadId,
  getEnvMessagePort
} = internalBinding('worker');

const { Readable, Writable } = require('stream');
const EventEmitter = require('events');
const { inspect } = require('internal/util/inspect');
const debug = require('internal/util/debuglog').debuglog('worker');

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

// We have to mess with the MessagePort prototype a bit, so that a) we can make
// it inherit from EventEmitter, even though it is a C++ class, and b) we do
// not provide methods that are not present in the Browser and not documented
// on our side (e.g. hasRef).
// Save a copy of the original set of methods as a shallow clone.
const MessagePortPrototype = ObjectCreate(
  ObjectGetPrototypeOf(MessagePort.prototype),
  ObjectGetOwnPropertyDescriptors(MessagePort.prototype));
// Set up the new inheritance chain.
ObjectSetPrototypeOf(MessagePort, EventEmitter);
ObjectSetPrototypeOf(MessagePort.prototype, EventEmitter.prototype);
// Copy methods that are inherited from HandleWrap, because
// changing the prototype of MessagePort.prototype implicitly removed them.
MessagePort.prototype.ref = MessagePortPrototype.ref;
MessagePort.prototype.unref = MessagePortPrototype.unref;

// A communication channel consisting of a handle (that wraps around an
// uv_async_t) which can receive information from other threads and emits
// .onmessage events, and a function used for sending data to a MessagePort
// in some other thread.
MessagePort.prototype[kOnMessageListener] = function onmessage(event) {
  if (event.data && event.data.type !== messageTypes.STDIO_WANTS_MORE_DATA)
    debug(`[${threadId}] received message`, event);
  // Emit the deserialized object to userland.
  this.emit('message', event.data);
};

// This is for compatibility with the Web's MessagePort API. It makes sense to
// provide it as an `EventEmitter` in Node.js, but if somebody overrides
// `onmessage`, we'll switch over to the Web API model.
ObjectDefineProperty(MessagePort.prototype, 'onmessage', {
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
      stopMessagePort(this);
    }
  }
});

// This is called from inside the `MessagePort` constructor.
function oninit() {
  setupPortReferencing(this, this, 'message');
}

ObjectDefineProperty(MessagePort.prototype, onInitSymbol, {
  enumerable: true,
  writable: false,
  value: oninit
});

// This is called after the underlying `uv_async_t` has been closed.
function onclose() {
  this.emit('close');
}

ObjectDefineProperty(MessagePort.prototype, handleOnCloseSymbol, {
  enumerable: false,
  writable: false,
  value: onclose
});

MessagePort.prototype.close = function(cb) {
  if (typeof cb === 'function')
    this.once('close', cb);
  MessagePortPrototype.close.call(this);
};

ObjectDefineProperty(MessagePort.prototype, inspect.custom, {
  enumerable: false,
  writable: false,
  value: function inspect() {  // eslint-disable-line func-name-matching
    let ref;
    try {
      // This may throw when `this` does not refer to a native object,
      // e.g. when accessing the prototype directly.
      ref = MessagePortPrototype.hasRef.call(this);
    } catch { return this; }
    return ObjectAssign(ObjectCreate(MessagePort.prototype),
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
      stopMessagePort(port);
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
      if (this[kStartedReading] && this[kIncrementsPortRef]) {
        if (--this[kPort][kWaitingStreams] === 0)
          this[kPort].unref();
      }
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

function createWorkerStdio() {
  const port = getEnvMessagePort();
  port[kWaitingStreams] = 0;
  return {
    stdin: new ReadableWorkerStdio(port, 'stdin'),
    stdout: new WritableWorkerStdio(port, 'stdout'),
    stderr: new WritableWorkerStdio(port, 'stderr')
  };
}

function receiveMessageOnPort(port) {
  const message = receiveMessageOnPort_(port);
  if (message === noMessageSymbol) return undefined;
  return { message };
}

module.exports = {
  drainMessagePort,
  messageTypes,
  kPort,
  kIncrementsPortRef,
  kWaitingStreams,
  kStdioWantsMoreDataCallback,
  moveMessagePortToContext,
  MessagePort,
  MessageChannel,
  receiveMessageOnPort,
  setupPortReferencing,
  ReadableWorkerStdio,
  WritableWorkerStdio,
  createWorkerStdio
};

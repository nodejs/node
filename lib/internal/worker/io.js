'use strict';

const {
  ArrayPrototypeForEach,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  FunctionPrototypeCall,
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptors,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  ReflectApply,
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
  getEnvMessagePort
} = internalBinding('worker');

const { Readable, Writable } = require('stream');
const {
  Event,
  EventTarget,
  NodeEventTarget,
  defineEventHandler,
  initNodeEventTarget,
  kCreateEvent,
  kNewListener,
  kRemoveListener,
} = require('internal/event_target');
const { inspect } = require('internal/util/inspect');

const kIncrementsPortRef = Symbol('kIncrementsPortRef');
const kName = Symbol('kName');
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
// it inherit from NodeEventTarget, even though it is a C++ class, and b) we do
// not provide methods that are not present in the Browser and not documented
// on our side (e.g. hasRef).
// Save a copy of the original set of methods as a shallow clone.
const MessagePortPrototype = ObjectCreate(
  ObjectGetPrototypeOf(MessagePort.prototype),
  ObjectGetOwnPropertyDescriptors(MessagePort.prototype));
// Set up the new inheritance chain.
ObjectSetPrototypeOf(MessagePort, NodeEventTarget);
ObjectSetPrototypeOf(MessagePort.prototype, NodeEventTarget.prototype);
// Copy methods that are inherited from HandleWrap, because
// changing the prototype of MessagePort.prototype implicitly removed them.
MessagePort.prototype.ref = MessagePortPrototype.ref;
MessagePort.prototype.unref = MessagePortPrototype.unref;

class MessageEvent extends Event {
  constructor(data, target, type) {
    super(type);
    this.data = data;
  }
}

const originalCreateEvent = EventTarget.prototype[kCreateEvent];
ObjectDefineProperty(
  MessagePort.prototype,
  kCreateEvent,
  {
    value: function(data, type) {
      if (type !== 'message' && type !== 'messageerror') {
        return ReflectApply(originalCreateEvent, this, arguments);
      }
      return new MessageEvent(data, this, type);
    },
    configurable: false,
    writable: false,
    enumerable: false,
  });

// This is called from inside the `MessagePort` constructor.
function oninit() {
  initNodeEventTarget(this);
  setupPortReferencing(this, this, 'message');
}

defineEventHandler(MessagePort.prototype, 'message');
defineEventHandler(MessagePort.prototype, 'messageerror');

ObjectDefineProperty(MessagePort.prototype, onInitSymbol, {
  enumerable: true,
  writable: false,
  value: oninit
});

class MessagePortCloseEvent extends Event {
  constructor() {
    super('close');
  }
}

// This is called after the underlying `uv_async_t` has been closed.
function onclose() {
  this.dispatchEvent(new MessagePortCloseEvent());
}

ObjectDefineProperty(MessagePort.prototype, handleOnCloseSymbol, {
  enumerable: false,
  writable: false,
  value: onclose
});

MessagePort.prototype.close = function(cb) {
  if (typeof cb === 'function')
    this.once('close', cb);
  FunctionPrototypeCall(MessagePortPrototype.close, this);
};

ObjectDefineProperty(MessagePort.prototype, inspect.custom, {
  enumerable: false,
  writable: false,
  value: function inspect() {  // eslint-disable-line func-name-matching
    let ref;
    try {
      // This may throw when `this` does not refer to a native object,
      // e.g. when accessing the prototype directly.
      ref = FunctionPrototypeCall(MessagePortPrototype.hasRef, this);
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
  eventEmitter.on('newListener', function(name) {
    if (name === eventName) newListener(eventEmitter.listenerCount(name));
  });
  eventEmitter.on('removeListener', function(name) {
    if (name === eventName) removeListener(eventEmitter.listenerCount(name));
  });
  const origNewListener = eventEmitter[kNewListener];
  eventEmitter[kNewListener] = function(size, type, ...args) {
    if (type === eventName) newListener(size - 1);
    return ReflectApply(origNewListener, this, arguments);
  };
  const origRemoveListener = eventEmitter[kRemoveListener];
  eventEmitter[kRemoveListener] = function(size, type, ...args) {
    if (type === eventName) removeListener(size);
    return ReflectApply(origRemoveListener, this, arguments);
  };

  function newListener(size) {
    if (size === 0) {
      port.ref();
      FunctionPrototypeCall(MessagePortPrototype.start, port);
    }
  }

  function removeListener(size) {
    if (size === 0) {
      stopMessagePort(port);
      port.unref();
    }
  }
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

  _writev(chunks, cb) {
    this[kPort].postMessage({
      type: messageTypes.STDIO_PAYLOAD,
      stream: this[kName],
      chunks: ArrayPrototypeMap(chunks,
                                ({ chunk, encoding }) => ({ chunk, encoding })),
    });
    ArrayPrototypePush(this[kWritableCallbacks], cb);
    if (this[kPort][kWaitingStreams]++ === 0)
      this[kPort].ref();
  }

  _final(cb) {
    this[kPort].postMessage({
      type: messageTypes.STDIO_PAYLOAD,
      stream: this[kName],
      chunks: [ { chunk: null, encoding: '' } ]
    });
    cb();
  }

  [kStdioWantsMoreDataCallback]() {
    const cbs = this[kWritableCallbacks];
    this[kWritableCallbacks] = [];
    ArrayPrototypeForEach(cbs, (cb) => cb());
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

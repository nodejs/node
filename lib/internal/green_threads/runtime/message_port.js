'use strict';

// MessageChannel, MessagePort and MessageEvent for green threads. The ports
// are Node.js' MessagePorts underneath, which src/node_green_thread.cc
// receives messages for, so they can connect green threads with each other,
// with their host and with workers.

const {
  ArrayPrototypeSlice,
  FunctionPrototypeCall,
  ObjectDefineProperties,
  ObjectFreeze,
  ReflectApply,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  messageChannel,
  portClose,
  portId,
  portPostMessage,
  portStart,
} = internalBinding('green_thread_runtime');
const {
  Event,
  EventTarget,
  markEventTrusted,
} = require('internal/green_threads/runtime/event_target');
const { illegalConstructor } = require('internal/green_threads/runtime/errors');

const kCreate = Symbol('kCreate');

class MessageEvent extends Event {
  #data;
  #ports;

  constructor(type, options = undefined) {
    super(type, options);
    this.#data = options?.data ?? null;
    this.#ports = ObjectFreeze(
      options?.ports == null ? [] : ArrayPrototypeSlice(options.ports));
  }

  get data() { return this.#data; }
  get ports() { return this.#ports; }
  get origin() { return ''; }
  get lastEventId() { return ''; }
  get source() { return null; }
}

ObjectDefineProperties(MessageEvent.prototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'MessageEvent',
    configurable: true,
  },
});

class MessagePort extends EventTarget {
  #handlers = { __proto__: null };

  constructor(key, id) {
    if (key !== kCreate) throw illegalConstructor();
    super();
    this[portId] = id;
  }

  postMessage(message, transfer = undefined) {
    portPostMessage(this, message, transfer);
  }

  start() {
    portStart(this);
  }

  close() {
    portClose(this);
  }

  // Like Node.js' MessagePorts, ports start once they have 'message'
  // listeners.
  addEventListener(type, listener, options = undefined) {
    ReflectApply(super.addEventListener, this, arguments);
    if (`${type}` === 'message' && listener != null) this.start();
  }

  get onmessage() { return this.#getHandler('message'); }
  set onmessage(handler) { this.#setHandler('message', handler); }
  get onmessageerror() { return this.#getHandler('messageerror'); }
  set onmessageerror(handler) { this.#setHandler('messageerror', handler); }

  #getHandler(type) {
    return this.#handlers[type]?.handler ?? null;
  }

  #setHandler(type, handler) {
    const previous = this.#handlers[type];
    if (previous !== undefined) {
      this.removeEventListener(type, previous.listener);
      delete this.#handlers[type];
    }
    if (typeof handler !== 'function') return;
    const listener = (event) => FunctionPrototypeCall(handler, this, event);
    this.#handlers[type] = { handler, listener };
    this.addEventListener(type, listener);
  }
}

ObjectDefineProperties(MessagePort.prototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'MessagePort',
    configurable: true,
  },
});

class MessageChannel {
  #port1;
  #port2;

  constructor() {
    const { 0: port1, 1: port2 } = messageChannel();
    this.#port1 = port1;
    this.#port2 = port2;
  }

  get port1() { return this.#port1; }
  get port2() { return this.#port2; }
}

ObjectDefineProperties(MessageChannel.prototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'MessageChannel',
    configurable: true,
  },
});

function dispatch(port, event) {
  markEventTrusted(event);
  port.dispatchEvent(event);
}

module.exports = {
  MessageChannel,
  MessageEvent,
  MessagePort,
  // Used by src/node_green_thread.cc.
  createMessagePort: (id) => new MessagePort(kCreate, id),
  dispatchPortMessage(port, data, ports, ok) {
    dispatch(port, new MessageEvent(ok ? 'message' : 'messageerror',
                                    { data, ports }));
  },
  dispatchPortClose(port) {
    dispatch(port, new Event('close'));
  },
};

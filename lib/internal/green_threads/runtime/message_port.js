'use strict';

// MessageChannel, MessagePort, BroadcastChannel and MessageEvent for green
// threads. The ports are Node.js' MessagePorts underneath, which
// src/node_green_thread.cc receives messages for, so they can connect green
// threads with each other, with their host and with workers. So are
// BroadcastChannels, which are those of Node.js, process-wide, for the names
// in the green thread's broadcastScopes.

const {
  ArrayPrototypeSlice,
  FunctionPrototypeCall,
  ObjectDefineProperties,
  ObjectFreeze,
  ReflectApply,
  Symbol,
  SymbolToStringTag,
} = primordials;

const { DOMException } = internalBinding('per_context');
const {
  broadcastChannel,
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
const {
  broadcastScope,
  illegalConstructor,
  missingArgs,
} = require('internal/green_threads/runtime/errors');

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

// An EventTarget with onmessage and onmessageerror.
class MessageEventTarget extends EventTarget {
  #handlers = { __proto__: null };

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

class MessagePort extends MessageEventTarget {
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

// Like Node.js' BroadcastChannel: messages from others with the same name,
// in any thread, are 'message' events, without waiting for listeners.
class BroadcastChannel extends MessageEventTarget {
  #name;
  #port;

  constructor(name) {
    if (arguments.length === 0) {
      throw missingArgs('The "name" argument must be specified');
    }
    super();
    this.#name = `${name}`;
    const port = broadcastChannel(this.#name);
    if (port === undefined) throw broadcastScope(this.#name);
    this.#port = port;
    const forward = (event) => {
      dispatch(this, new MessageEvent(event.type, { data: event.data }));
    };
    port.addEventListener('message', forward);
    port.addEventListener('messageerror', forward);
  }

  get name() { return this.#name; }

  close() {
    if (this.#port === undefined) return;
    this.#port.close();
    this.#port = undefined;
  }

  postMessage(message) {
    if (arguments.length === 0) {
      throw missingArgs('The "message" argument must be specified');
    }
    if (this.#port === undefined) {
      throw new DOMException('BroadcastChannel is closed.',
                             'InvalidStateError');
    }
    portPostMessage(this.#port, message, undefined);
  }
}

ObjectDefineProperties(BroadcastChannel.prototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'BroadcastChannel',
    configurable: true,
  },
});

function dispatch(port, event) {
  markEventTrusted(event);
  port.dispatchEvent(event);
}

module.exports = {
  BroadcastChannel,
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

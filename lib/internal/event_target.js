'use strict';

const {
  ArrayFrom,
  Boolean,
  Error,
  Map,
  NumberIsInteger,
  Object,
  Symbol,
  SymbolFor,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_EVENT_RECURSION,
    ERR_MISSING_ARGS,
    ERR_INVALID_THIS,
  }
} = require('internal/errors');
const { validateInteger, validateObject } = require('internal/validators');

const { customInspectSymbol } = require('internal/util');
const { inspect } = require('util');

const kIsEventTarget = SymbolFor('nodejs.event_target');

const kEvents = Symbol('kEvents');
const kStop = Symbol('kStop');
const kTarget = Symbol('kTarget');

const kNewListener = Symbol('kNewListener');
const kRemoveListener = Symbol('kRemoveListener');

// Lazy load perf_hooks to avoid the additional overhead on startup
let perf_hooks;
function lazyNow() {
  if (perf_hooks === undefined)
    perf_hooks = require('perf_hooks');
  return perf_hooks.performance.now();
}

class Event {
  #type = undefined;
  #defaultPrevented = false;
  #cancelable = false;
  #timestamp = lazyNow();

  // None of these are currently used in the Node.js implementation
  // of EventTarget because there is no concept of bubbling or
  // composition. We preserve their values in Event but they are
  // non-ops and do not carry any semantics in Node.js
  #bubbles = false;
  #composed = false;
  #propagationStopped = false;


  constructor(type, options) {
    if (arguments.length === 0)
      throw new ERR_MISSING_ARGS('type');
    if (options != null)
      validateObject(options, 'options');
    const { cancelable, bubbles, composed } = { ...options };
    this.#cancelable = !!cancelable;
    this.#bubbles = !!bubbles;
    this.#composed = !!composed;
    this.#type = `${type}`;
    this.#propagationStopped = false;
    // isTrusted is special (LegacyUnforgeable)
    Object.defineProperty(this, 'isTrusted', {
      get() { return false; },
      enumerable: true,
      configurable: false
    });
    this[kTarget] = null;
  }

  [customInspectSymbol](depth, options) {
    const name = this.constructor.name;
    if (depth < 0)
      return name;

    const opts = Object.assign({}, options, {
      depth: NumberIsInteger(options.depth) ? options.depth - 1 : options.depth
    });

    return `${name} ${inspect({
      type: this.#type,
      defaultPrevented: this.#defaultPrevented,
      cancelable: this.#cancelable,
      timeStamp: this.#timestamp,
    }, opts)}`;
  }

  stopImmediatePropagation() {
    this[kStop] = true;
  }

  preventDefault() {
    this.#defaultPrevented = true;
  }

  get target() { return this[kTarget]; }
  get currentTarget() { return this[kTarget]; }
  get srcElement() { return this[kTarget]; }

  get type() { return this.#type; }

  get cancelable() { return this.#cancelable; }

  get defaultPrevented() { return this.#cancelable && this.#defaultPrevented; }

  get timeStamp() { return this.#timestamp; }


  // The following are non-op and unused properties/methods from Web API Event.
  // These are not supported in Node.js and are provided purely for
  // API completeness.

  composedPath() { return this[kTarget] ? [this[kTarget]] : []; }
  get returnValue() { return !this.defaultPrevented; }
  get bubbles() { return this.#bubbles; }
  get composed() { return this.#composed; }
  get eventPhase() {
    return this[kTarget] ? Event.AT_TARGET : Event.NONE;
  }
  get cancelBubble() { return this.#propagationStopped; }
  set cancelBubble(value) {
    if (value) {
      this.stopPropagation();
    }
  }
  stopPropagation() {
    this.#propagationStopped = true;
  }

  static NONE = 0;
  static CAPTURING_PHASE = 1;
  static AT_TARGET = 2;
  static BUBBLING_PHASE = 3;
}

Object.defineProperty(Event.prototype, SymbolToStringTag, {
  writable: false,
  enumerable: false,
  configurable: true,
  value: 'Event',
});

class EventTarget {
  // Used in checking whether an object is an EventTarget. This is a well-known
  // symbol as EventTarget may be used cross-realm. See discussion in #33661.
  static [kIsEventTarget] = true;

  // eventListeners are stored in a hash table data structure. It is optimized
  // for singular listeners by mapping type to handler directly, but if another
  // listener is added, it turns into an array and handles them iteratively
  [kEvents] = new Map();

  [kNewListener](size, type, listener, once, capture, passive) {}
  [kRemoveListener](size, type, listener, capture) {}

  same([ listener1, capture1 ], [ listener2, capture2 ]) {
    return listener1 === listener2 && capture1 === capture2;
  }

  createCallback(listener) {
    return typeof listener === 'function' ? listener :
      listener.handleEvent.bind(listener);
  }

  addEventListener(type, listener, options = {}) {
    if (arguments.length < 2)
      throw new ERR_MISSING_ARGS('type', 'listener');

    // We validateOptions before the shouldAddListeners check because the spec
    // requires us to hit getters.
    const {
      once,
      capture,
      passive
    } = validateEventListenerOptions(options);

    if (!shouldAddListener(listener)) {
      // The DOM silently allows passing undefined as a second argument
      // No error code for this since it is a Warning
      // eslint-disable-next-line no-restricted-syntax
      const w = new Error(
        `addEventListener called with ${listener} which has no effect.`
      );
      w.name = 'AddEventListenerArgumentTypeWarning';
      w.target = this;
      w.type = type;
      process.emitWarning(w);
      return;
    }
    type = String(type);

    const listeners = this[kEvents].get(type);

    if (listeners === undefined) {
      this[kNewListener](1, type, listener, once, capture, passive);
      this[kEvents].set(type, [
        [listener, once, capture, passive, this.createCallback(listener)]
      ]);
      return;
    }

    for (let i = 0; i < listeners; i++) {
      if (this.same([listener, capture], listeners[i])) { // Duplicate! Ignore
        return;
      }
      listeners.push(
        [listener, once, capture, passive, this.createCallback(listener)]
      );
      this[kNewListener](
        listeners.length + 1,
        type, listener, once, capture, passive
      );
    }
  }

  removeEventListener(type, listener, options = {}) {
    if (!shouldAddListener(listener))
      return;

    type = String(type);
    // TODO(@jasnell): If it's determined this cannot be backported
    // to 12.x, then this can be simplified to:
    //   const capture = Boolean(options?.capture);
    const capture = options != null && options.capture === true;

    const listeners = this[kEvents].get(type);
    if (listeners === undefined)
      return;

    for (let i = 0; i < listeners.length; i++) {
      if (this.same([listener, capture], listeners[i])) {
        listeners.splice(i, 1);
        break;
      }
    }

    if (listeners.length === 0) {
      this[kEvents].delete(type);
    }

    this[kRemoveListener](listeners.length, type, listener, capture);
  }

  dispatchEvent(event) {
    if (!(event instanceof Event))
      throw new ERR_INVALID_ARG_TYPE('event', 'Event', event);

    if (!isEventTarget(this))
      throw new ERR_INVALID_THIS('EventTarget');

    if (event[kTarget] !== null)
      throw new ERR_EVENT_RECURSION(event.type);

    const listeners = this[kEvents].get(event.type);
    if (listeners === undefined) {
      return true;
    }

    event[kTarget] = this;

    function executeHandler(callback) {
      try {
        const result = callback.call(this, event);
        if (result !== undefined && result !== null)
          addCatch(this, result, event);
      } catch (err) {
        emitUnhandledRejectionOrErr(this, err, event);
      }
    }

    for (let i = 0; i < listeners.length; i++) {
      const [,, once,, callback] = listeners[i];

      if (once) {
        listeners.splice(i, 1);
      }

      executeHandler(callback);
    }
    event[kTarget] = undefined;

    return event.defaultPrevented !== true;
  }

  [customInspectSymbol](depth, options) {
    const name = this.constructor.name;
    if (depth < 0)
      return name;

    const opts = Object.assign({}, options, {
      depth: NumberIsInteger(options.depth) ? options.depth - 1 : options.depth
    });

    return `${name} ${inspect({}, opts)}`;
  }
}

Object.defineProperties(EventTarget.prototype, {
  addEventListener: { enumerable: true },
  removeEventListener: { enumerable: true },
  dispatchEvent: { enumerable: true }
});
Object.defineProperty(EventTarget.prototype, SymbolToStringTag, {
  writable: false,
  enumerable: false,
  configurable: true,
  value: 'EventTarget',
});

class NodeEventTarget extends EventTarget {
  static defaultMaxListeners = 10;

  #maxListeners = NodeEventTarget.defaultMaxListeners;
  #maxListenersWarned = false;

  [kNewListener](size, type, listener, once, capture, passive) {
    if (this.#maxListeners > 0 &&
        size > this.#maxListeners &&
        !this.#maxListenersWarned) {
      this.#maxListenersWarned = true;
      // No error code for this since it is a Warning
      // eslint-disable-next-line no-restricted-syntax
      const w = new Error('Possible EventTarget memory leak detected. ' +
                          `${size} ${type} listeners ` +
                          `added to ${inspect(this, { depth: -1 })}. Use ` +
                          'setMaxListeners() to increase limit');
      w.name = 'MaxListenersExceededWarning';
      w.target = this;
      w.type = type;
      w.count = size;
      process.emitWarning(w);
    }
  }

  setMaxListeners(n) {
    validateInteger(n, 'n', 0);
    this.#maxListeners = n;
    return this;
  }

  getMaxListeners() {
    return this.#maxListeners;
  }

  eventNames() {
    return ArrayFrom(this[kEvents].keys());
  }

  listenerCount(type) {
    const listeners = this[kEvents].get(String(type));
    return listeners !== undefined ? listeners.length : 0;
  }

  off(type, listener, options) {
    this.removeEventListener(type, listener, options);
    return this;
  }

  removeListener(type, listener, options) {
    this.removeEventListener(type, listener, options);
    return this;
  }

  on(type, listener) {
    this.addEventListener(type, listener);
    return this;
  }

  addListener(type, listener) {
    this.addEventListener(type, listener);
    return this;
  }

  once(type, listener) {
    this.addEventListener(type, listener, { once: true });
    return this;
  }

  removeAllListeners(type) {
    if (type !== undefined) {
      this[kEvents].delete(String(type));
    } else {
      this[kEvents].clear();
    }
  }
}

Object.defineProperties(NodeEventTarget.prototype, {
  setMaxListeners: { enumerable: true },
  getMaxListeners: { enumerable: true },
  eventNames: { enumerable: true },
  listenerCount: { enumerable: true },
  off: { enumerable: true },
  removeListener: { enumerable: true },
  on: { enumerable: true },
  addListener: { enumerable: true },
  once: { enumerable: true },
  removeAllListeners: { enumerable: true },
});

// EventTarget API

function shouldAddListener(listener) {
  if (typeof listener === 'function' ||
      (listener != null &&
       typeof listener === 'object' &&
       typeof listener.handleEvent === 'function')) {
    return true;
  }

  if (listener == null)
    return false;

  throw new ERR_INVALID_ARG_TYPE('listener', 'EventListener', listener);
}

function validateEventListenerOptions(options) {
  if (typeof options === 'boolean')
    return { capture: options };
  validateObject(options, 'options');
  return {
    once: Boolean(options.once),
    capture: Boolean(options.capture),
    passive: Boolean(options.passive),
  };
}

// Test whether the argument is an event object. This is far from a fool-proof
// test, for example this input will result in a false positive:
// > isEventTarget({ constructor: EventTarget })
// It stands in its current implementation as a compromise. For the relevant
// discussion, see #33661.
function isEventTarget(obj) {
  return obj && obj.constructor && obj.constructor[kIsEventTarget];
}

function addCatch(that, promise, event) {
  const then = promise.then;
  if (typeof then === 'function') {
    then.call(promise, undefined, function(err) {
      // The callback is called with nextTick to avoid a follow-up
      // rejection from this promise.
      process.nextTick(emitUnhandledRejectionOrErr, that, err, event);
    });
  }
}

function emitUnhandledRejectionOrErr(that, err, event) {
  process.emit('error', err, event);
}

function defineEventHandler(emitter, name) {
  // 8.1.5.1 Event handlers - basically `on[eventName]` attributes
  let eventHandlerValue;
  Object.defineProperty(emitter, `on${name}`, {
    get() {
      return eventHandlerValue;
    },
    set(value) {
      if (eventHandlerValue) {
        emitter.removeEventListener(name, eventHandlerValue);
      }
      if (typeof value === 'function') {
        emitter.addEventListener(name, value);
      }
      eventHandlerValue = value;
    }
  });
}
module.exports = {
  Event,
  EventTarget,
  NodeEventTarget,
  defineEventHandler,
};

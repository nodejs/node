'use strict';

const {
  Map,
  Object,
  Set,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_EVENT_RECURSION,
  }
} = require('internal/errors');

const { customInspectSymbol } = require('internal/util');
const { inspect } = require('util');

const kEvents = Symbol('kEvents');
const kStop = Symbol('kStop');
const kTarget = Symbol('kTarget');

const kNewListener = Symbol('kNewListener');
const kRemoveListener = Symbol('kRemoveListener');

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

  // Neither of these are currently used in the Node.js implementation
  // of EventTarget because there is no concept of bubbling or
  // composition. We preserve their values in Event but they are
  // non-ops and do not carry any semantics in Node.js
  #bubbles = false;
  #composed = false;


  constructor(type, options) {
    if (options != null && typeof options !== 'object')
      throw new ERR_INVALID_ARG_TYPE('options', 'object', options);
    const { cancelable, bubbles, composed } = { ...options };
    this.#cancelable = !!cancelable;
    this.#bubbles = !!bubbles;
    this.#composed = !!composed;
    this.#type = String(type);
    // isTrusted is special (LegacyUnforgeable)
    Object.defineProperty(this, 'isTrusted', {
      get() { return false; },
      set(ignoredValue) { return false; },
      enumerable: true,
      configurable: false
    });
  }

  [customInspectSymbol](depth, options) {
    const name = this.constructor.name;
    if (depth < 0)
      return name;

    const opts = Object.assign({}, options, {
      dept: options.depth === null ? null : options.depth - 1
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
    return this[kTarget] ? 2 : 0;  // Equivalent to AT_TARGET or NONE
  }
  cancelBubble() {
    // Non-op in Node.js. Alias for stopPropagation
  }
  stopPropagation() {
    // Non-op in Node.js
  }

  get [Symbol.toStringTag]() { return 'Event'; }
}

// The listeners for an EventTarget are maintained as a linked list.
// Unfortunately, the way EventTarget is defined, listeners are accounted
// using the tuple [handler,capture], and even if we don't actually make
// use of capture or bubbling, in order to be spec compliant we have to
// take on the additional complexity of supporting it. Fortunately, using
// the linked list makes dispatching faster, even if adding/removing is
// slower.
class Listener {
  next;
  previous;
  listener;
  callback;
  once;
  capture;
  passive;

  constructor(previous, listener, once, capture, passive) {
    if (previous !== undefined)
      previous.next = this;
    this.previous = previous;
    this.listener = listener;
    this.once = once;
    this.capture = capture;
    this.passive = passive;

    this.callback =
      typeof listener === 'function' ?
        listener :
        listener.handleEvent.bind(listener);
  }

  same(listener, capture) {
    return this.listener === listener && this.capture === capture;
  }

  remove() {
    if (this.previous !== undefined)
      this.previous.next = this.next;
    if (this.next !== undefined)
      this.next.previous = this.previous;
  }
}

class EventTarget {
  [kEvents] = new Map();
  #emitting = new Set();

  [kNewListener](size, type, listener, once, capture, passive) {}
  [kRemoveListener](size, type, listener, capture) {}

  addEventListener(type, listener, options = {}) {
    validateListener(listener);
    type = String(type);

    const {
      once,
      capture,
      passive
    } = validateEventListenerOptions(options);

    let root = this[kEvents].get(type);

    if (root === undefined) {
      root = { size: 1, next: undefined };
      // This is the first handler in our linked list.
      new Listener(root, listener, once, capture, passive);
      this[kNewListener](root.size, type, listener, once, capture, passive);
      this[kEvents].set(type, root);
      return;
    }

    let handler = root.next;
    let previous;

    // We have to walk the linked list to see if we have a match
    while (handler !== undefined && !handler.same(listener, capture)) {
      previous = handler;
      handler = handler.next;
    }

    if (handler !== undefined) { // Duplicate! Ignore
      return;
    }

    new Listener(previous, listener, once, capture, passive);
    root.size++;
    this[kNewListener](root.size, type, listener, once, capture, passive);
  }

  removeEventListener(type, listener, options = {}) {
    validateListener(listener);
    type = String(type);
    const { capture } = validateEventListenerOptions(options);
    const root = this[kEvents].get(type);
    if (root === undefined || root.next === undefined)
      return;

    let handler = root.next;
    while (handler !== undefined) {
      if (handler.same(listener, capture)) {
        handler.remove();
        root.size--;
        if (root.size === 0)
          this[kEvents].delete(type);
        this[kRemoveListener](root.size, type, listener, capture);
        break;
      }
      handler = handler.next;
    }
  }

  dispatchEvent(event) {
    if (!(event instanceof Event)) {
      throw new ERR_INVALID_ARG_TYPE('event', 'Event', event);
    }

    if (this.#emitting.has(event.type) ||
        event[kTarget] !== undefined) {
      throw new ERR_EVENT_RECURSION(event.type);
    }

    const root = this[kEvents].get(event.type);
    if (root === undefined || root.next === undefined)
      return true;

    event[kTarget] = this;
    this.#emitting.add(event.type);

    let handler = root.next;
    let next;

    while (handler !== undefined &&
           (handler.passive || event[kStop] !== true)) {
      // Cache the next item in case this iteration removes the current one
      next = handler.next;

      if (handler.once) {
        handler.remove();
        root.size--;
      }

      try {
        const result = handler.callback.call(this, event);
        if (result !== undefined && result !== null)
          addCatch(this, result, event);
      } catch (err) {
        emitUnhandledRejectionOrErr(this, err, event);
      }

      handler = next;
    }

    this.#emitting.delete(event.type);
    event[kTarget] = undefined;

    return event.defaultPrevented === true ? false : true;
  }

  [customInspectSymbol](depth, options) {
    const name = this.constructor.name;
    if (depth < 0)
      return name;

    const opts = Object.assign({}, options, {
      dept: options.depth === null ? null : options.depth - 1
    });

    return `${name} ${inspect({}, opts)}`;
  }
  get [Symbol.toStringTag]() { return 'EventTarget'; }
}

Object.defineProperties(EventTarget.prototype, {
  addEventListener: { enumerable: true },
  removeEventListener: { enumerable: true },
  dispatchEvent: { enumerable: true }
});

function validateListener(listener) {
  if (typeof listener === 'function' ||
      (listener != null &&
       typeof listener === 'object' &&
       typeof listener.handleEvent === 'function')) {
    return;
  }
  throw new ERR_INVALID_ARG_TYPE('listener', 'EventListener', listener);
}

function validateEventListenerOptions(options) {
  if (options == null || typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'object', options);
  const {
    once = false,
    capture = false,
    passive = false,
  } = options;
  return {
    once: !!once,
    capture: !!capture,
    passive: !!passive,
  };
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

module.exports = {
  Event,
  EventTarget,
};

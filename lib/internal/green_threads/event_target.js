'use strict';

// Event, EventTarget, AbortSignal and AbortController for green threads.
// There is no DOM tree, so events are only ever dispatched to their target.
// Node.js' own implementation depends on too much of Node.js to run in a
// green thread.

const {
  ArrayPrototypeFindIndex,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  DateNow,
  FunctionPrototypeCall,
  NumberIsNaN,
  ObjectDefineProperties,
  SafeMap,
  Symbol,
  SymbolToStringTag,
  globalThis,
} = primordials;

// Taken before any user code runs.
const { queueMicrotask, setTimeout } = globalThis;

const { DOMException } = internalBinding('per_context');
const {
  illegalConstructor,
  invalidArgType,
  missingArgs,
  outOfRange,
} = require('internal/green_threads/errors');

const kCreate = Symbol('kCreate');
const timeOrigin = DateNow();

let setEventTarget;
let setEventCurrentTarget;
let setEventPhase;
let setEventTrusted;
let setEventDispatching;
let isEventDispatching;
let isEventStopped;
let isEvent;

class Event {
  #type;
  #bubbles;
  #cancelable;
  #composed;
  #timeStamp = DateNow() - timeOrigin;
  #defaultPrevented = false;
  #target = null;
  #currentTarget = null;
  #eventPhase = 0;
  #isTrusted = false;
  #dispatching = false;
  #propagationStopped = false;
  #immediatePropagationStopped = false;
  #inPassiveListener = false;

  constructor(type, options = undefined) {
    if (arguments.length === 0) {
      throw missingArgs('The "type" argument must be specified');
    }
    if (options != null && typeof options !== 'object') {
      throw invalidArgType('The "options" argument must be an object');
    }
    this.#type = `${type}`;
    this.#bubbles = !!options?.bubbles;
    this.#cancelable = !!options?.cancelable;
    this.#composed = !!options?.composed;
  }

  get type() { return this.#type; }
  get target() { return this.#target; }
  get srcElement() { return this.#target; }
  get currentTarget() { return this.#currentTarget; }
  get eventPhase() { return this.#eventPhase; }
  get bubbles() { return this.#bubbles; }
  get cancelable() { return this.#cancelable; }
  get composed() { return this.#composed; }
  get defaultPrevented() { return this.#cancelable && this.#defaultPrevented; }
  get timeStamp() { return this.#timeStamp; }
  get isTrusted() { return this.#isTrusted; }
  get returnValue() { return !this.defaultPrevented; }
  set returnValue(value) { if (!value) this.preventDefault(); }
  get cancelBubble() { return this.#propagationStopped; }
  set cancelBubble(value) { if (value) this.#propagationStopped = true; }

  composedPath() {
    return this.#dispatching ? [this.#currentTarget] : [];
  }

  stopPropagation() {
    this.#propagationStopped = true;
  }

  stopImmediatePropagation() {
    this.#propagationStopped = true;
    this.#immediatePropagationStopped = true;
  }

  preventDefault() {
    if (this.#cancelable && !this.#inPassiveListener) {
      this.#defaultPrevented = true;
    }
  }

  static {
    setEventTarget = (event, target) => { event.#target = target; };
    setEventCurrentTarget = (event, target) => {
      event.#currentTarget = target;
    };
    setEventPhase = (event, phase) => { event.#eventPhase = phase; };
    setEventTrusted = (event) => { event.#isTrusted = true; };
    setEventDispatching = (event, dispatching, passive = false) => {
      event.#dispatching = dispatching;
      event.#inPassiveListener = passive;
    };
    isEventDispatching = (event) => event.#dispatching;
    isEventStopped = (event) => event.#immediatePropagationStopped;
    isEvent = (value) => typeof value === 'object' && value !== null &&
      #type in value;
  }
}

ObjectDefineProperties(Event, {
  NONE: { __proto__: null, value: 0, enumerable: true },
  CAPTURING_PHASE: { __proto__: null, value: 1, enumerable: true },
  AT_TARGET: { __proto__: null, value: 2, enumerable: true },
  BUBBLING_PHASE: { __proto__: null, value: 3, enumerable: true },
});
ObjectDefineProperties(Event.prototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'Event',
    configurable: true,
  },
});

function flattenOptions(options) {
  if (typeof options === 'boolean') return { capture: options };
  if (options == null) return { capture: false };
  if (typeof options !== 'object') {
    throw invalidArgType('The "options" argument must be an object');
  }
  return {
    capture: !!options.capture,
    once: !!options.once,
    passive: !!options.passive,
    signal: options.signal,
  };
}

// Errors thrown by listeners don't stop the dispatch. They are reported like
// any other uncaught exception.
function reportListenerError(error) {
  queueMicrotask(() => { throw error; });
}

class EventTarget {
  // Event type => listener records, in the order they were added.
  #listeners = new SafeMap();

  addEventListener(type, callback, options = undefined) {
    if (arguments.length < 2) {
      throw missingArgs('The "type" and "listener" arguments must be specified');
    }
    const { capture, once, passive, signal } = flattenOptions(options);
    if (signal !== undefined && !isAbortSignal(signal)) {
      throw invalidArgType('The "options.signal" property must be an AbortSignal');
    }
    if (callback == null || signal?.aborted) return;
    if (typeof callback !== 'function' && typeof callback !== 'object') {
      throw invalidArgType('The "listener" argument must be a function or an object');
    }
    type = `${type}`;
    let listeners = this.#listeners.get(type);
    if (listeners === undefined) {
      listeners = [];
      this.#listeners.set(type, listeners);
    }
    const index = ArrayPrototypeFindIndex(listeners, (listener) => {
      return listener.callback === callback && listener.capture === capture;
    });
    if (index !== -1) return;
    ArrayPrototypePush(listeners, {
      callback,
      capture,
      once,
      passive,
      removed: false,
    });
    if (signal !== undefined) {
      FunctionPrototypeCall(
        EventTarget.prototype.addEventListener, signal, 'abort', () => {
          this.removeEventListener(type, callback, { capture });
        }, { once: true });
    }
  }

  removeEventListener(type, callback, options = undefined) {
    if (arguments.length < 2) {
      throw missingArgs('The "type" and "listener" arguments must be specified');
    }
    const { capture } = flattenOptions(options);
    const listeners = this.#listeners.get(`${type}`);
    if (listeners === undefined) return;
    const index = ArrayPrototypeFindIndex(listeners, (listener) => {
      return listener.callback === callback && listener.capture === capture;
    });
    if (index === -1) return;
    listeners[index].removed = true;
    ArrayPrototypeSplice(listeners, index, 1);
  }

  dispatchEvent(event) {
    if (!isEvent(event)) {
      throw invalidArgType('The "event" argument must be an Event');
    }
    if (isEventDispatching(event)) {
      throw new DOMException('The event is already being dispatched',
                             'InvalidStateError');
    }
    setEventTarget(event, this);
    setEventCurrentTarget(event, this);
    setEventPhase(event, Event.AT_TARGET);
    const listeners = this.#listeners.get(event.type);
    for (const listener of ArrayPrototypeSlice(listeners ?? [])) {
      if (listener.removed) continue;
      if (listener.once) {
        this.removeEventListener(event.type, listener.callback,
                                 { capture: listener.capture });
      }
      setEventDispatching(event, true, listener.passive);
      try {
        const { callback } = listener;
        if (typeof callback === 'function') {
          FunctionPrototypeCall(callback, this, event);
        } else if (typeof callback?.handleEvent === 'function') {
          FunctionPrototypeCall(callback.handleEvent, callback, event);
        }
      } catch (error) {
        reportListenerError(error);
      }
      if (isEventStopped(event)) break;
    }
    setEventDispatching(event, false);
    setEventCurrentTarget(event, null);
    setEventPhase(event, Event.NONE);
    return !event.defaultPrevented;
  }
}

ObjectDefineProperties(EventTarget.prototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'EventTarget',
    configurable: true,
  },
});

let isAbortSignal;
let abortSignal;

class AbortSignal extends EventTarget {
  #aborted = false;
  #reason = undefined;
  #onabort = null;
  #onabortListener = null;

  constructor(key) {
    if (key !== kCreate) throw illegalConstructor();
    super();
  }

  get aborted() { return this.#aborted; }
  get reason() { return this.#reason; }

  throwIfAborted() {
    if (this.#aborted) throw this.#reason;
  }

  get onabort() { return this.#onabort; }
  set onabort(handler) {
    if (this.#onabortListener !== null) {
      this.removeEventListener('abort', this.#onabortListener);
      this.#onabortListener = null;
    }
    this.#onabort = typeof handler === 'function' ? handler : null;
    if (this.#onabort !== null) {
      this.#onabortListener = (event) => {
        FunctionPrototypeCall(this.#onabort, this, event);
      };
      this.addEventListener('abort', this.#onabortListener);
    }
  }

  static abort(reason = new DOMException('This operation was aborted',
                                         'AbortError')) {
    const signal = new AbortSignal(kCreate);
    signal.#aborted = true;
    signal.#reason = reason;
    return signal;
  }

  static timeout(milliseconds) {
    milliseconds = +milliseconds;
    if (NumberIsNaN(milliseconds) || milliseconds < 0) {
      throw outOfRange('The "milliseconds" argument must be a non-negative number');
    }
    const signal = new AbortSignal(kCreate);
    setTimeout(() => {
      abortSignal(signal, new DOMException(
        'The operation was aborted due to timeout', 'TimeoutError'));
    }, milliseconds);
    return signal;
  }

  static any(signals) {
    const sources = [];
    for (const source of signals) {
      if (!isAbortSignal(source)) {
        throw invalidArgType('The "signals" argument must be an iterable of AbortSignals');
      }
      ArrayPrototypePush(sources, source);
    }
    const signal = new AbortSignal(kCreate);
    for (const source of sources) {
      if (source.#aborted) {
        signal.#aborted = true;
        signal.#reason = source.#reason;
        return signal;
      }
    }
    for (const source of sources) {
      FunctionPrototypeCall(
        EventTarget.prototype.addEventListener, source, 'abort', () => {
          abortSignal(signal, source.#reason);
        }, { once: true });
    }
    return signal;
  }

  static {
    isAbortSignal = (value) => typeof value === 'object' && value !== null &&
      #aborted in value;
    abortSignal = (signal, reason) => {
      if (signal.#aborted) return;
      signal.#aborted = true;
      signal.#reason = reason;
      const event = new Event('abort');
      setEventTrusted(event);
      signal.dispatchEvent(event);
    };
  }
}

ObjectDefineProperties(AbortSignal.prototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'AbortSignal',
    configurable: true,
  },
});

class AbortController {
  #signal = new AbortSignal(kCreate);

  get signal() { return this.#signal; }

  abort(reason = new DOMException('This operation was aborted',
                                  'AbortError')) {
    abortSignal(this.#signal, reason);
  }
}

ObjectDefineProperties(AbortController.prototype, {
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'AbortController',
    configurable: true,
  },
});

module.exports = {
  AbortController,
  AbortSignal,
  Event,
  EventTarget,
  abortSignal,
  createAbortSignal: () => new AbortSignal(kCreate),
  isAbortSignal,
};

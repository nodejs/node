'use strict';

const {
  ArrayFrom,
  Boolean,
  Error,
  FunctionPrototypeCall,
  NumberIsInteger,
  ObjectAssign,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyDescriptors,
  ReflectApply,
  SafeMap,
  String,
  Symbol,
  SymbolFor,
  SymbolToStringTag,
  SafeWeakSet,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_EVENT_RECURSION,
    ERR_MISSING_ARGS,
    ERR_INVALID_THIS,
  }
} = require('internal/errors');
const { validateObject } = require('internal/validators');

const { customInspectSymbol, kEventListener } = require('internal/util');
const { inspect } = require('util');

const kIsEventTarget = SymbolFor('nodejs.event_target');

const EventEmitter = require('events');
const {
  kMaxEventTargetListeners,
  kMaxEventTargetListenersWarned,
} = EventEmitter;

const kEvents = Symbol('kEvents');
const kStop = Symbol('kStop');
const kTarget = Symbol('kTarget');
const kHandlers = Symbol('khandlers');

const kHybridDispatch = SymbolFor('nodejs.internal.kHybridDispatch');
const kCreateEvent = Symbol('kCreateEvent');
const kNewListener = Symbol('kNewListener');
const kRemoveListener = Symbol('kRemoveListener');
const kIsNodeStyleListener = Symbol('kIsNodeStyleListener');
const kTrustEvent = Symbol('kTrustEvent');

// Lazy load perf_hooks to avoid the additional overhead on startup
let perf_hooks;
function lazyNow() {
  if (perf_hooks === undefined)
    perf_hooks = require('perf_hooks');
  return perf_hooks.performance.now();
}

// TODO(joyeecheung): V8 snapshot does not support instance member
// initializers for now:
// https://bugs.chromium.org/p/v8/issues/detail?id=10704
const kType = Symbol('type');
const kDefaultPrevented = Symbol('defaultPrevented');
const kCancelable = Symbol('cancelable');
const kTimestamp = Symbol('timestamp');
const kBubbles = Symbol('bubbles');
const kComposed = Symbol('composed');
const kPropagationStopped = Symbol('propagationStopped');

const isTrustedSet = new SafeWeakSet();
const isTrusted = ObjectGetOwnPropertyDescriptor({
  get isTrusted() {
    return isTrustedSet.has(this);
  }
}, 'isTrusted').get;

class Event {
  constructor(type, options = null) {
    if (arguments.length === 0)
      throw new ERR_MISSING_ARGS('type');
    if (options !== null)
      validateObject(options, 'options');
    const { cancelable, bubbles, composed } = { ...options };
    this[kCancelable] = !!cancelable;
    this[kBubbles] = !!bubbles;
    this[kComposed] = !!composed;
    this[kType] = `${type}`;
    this[kDefaultPrevented] = false;
    this[kTimestamp] = lazyNow();
    this[kPropagationStopped] = false;
    if (options != null && options[kTrustEvent]) {
      isTrustedSet.add(this);
    }

    // isTrusted is special (LegacyUnforgeable)
    ObjectDefineProperty(this, 'isTrusted', {
      get: isTrusted,
      enumerable: true,
      configurable: false
    });
    this[kTarget] = null;
  }

  [customInspectSymbol](depth, options) {
    const name = this.constructor.name;
    if (depth < 0)
      return name;

    const opts = ObjectAssign({}, options, {
      depth: NumberIsInteger(options.depth) ? options.depth - 1 : options.depth
    });

    return `${name} ${inspect({
      type: this[kType],
      defaultPrevented: this[kDefaultPrevented],
      cancelable: this[kCancelable],
      timeStamp: this[kTimestamp],
    }, opts)}`;
  }

  stopImmediatePropagation() {
    this[kStop] = true;
  }

  preventDefault() {
    this[kDefaultPrevented] = true;
  }

  get target() { return this[kTarget]; }
  get currentTarget() { return this[kTarget]; }
  get srcElement() { return this[kTarget]; }

  get type() { return this[kType]; }

  get cancelable() { return this[kCancelable]; }

  get defaultPrevented() {
    return this[kCancelable] && this[kDefaultPrevented];
  }

  get timeStamp() { return this[kTimestamp]; }


  // The following are non-op and unused properties/methods from Web API Event.
  // These are not supported in Node.js and are provided purely for
  // API completeness.

  composedPath() { return this[kTarget] ? [this[kTarget]] : []; }
  get returnValue() { return !this.defaultPrevented; }
  get bubbles() { return this[kBubbles]; }
  get composed() { return this[kComposed]; }
  get eventPhase() {
    return this[kTarget] ? Event.AT_TARGET : Event.NONE;
  }
  get cancelBubble() { return this[kPropagationStopped]; }
  set cancelBubble(value) {
    if (value) {
      this.stopPropagation();
    }
  }
  stopPropagation() {
    this[kPropagationStopped] = true;
  }

  static NONE = 0;
  static CAPTURING_PHASE = 1;
  static AT_TARGET = 2;
  static BUBBLING_PHASE = 3;
}

ObjectDefineProperty(Event.prototype, SymbolToStringTag, {
  writable: false,
  enumerable: false,
  configurable: true,
  value: 'Event',
});

class NodeCustomEvent extends Event {
  constructor(type, options) {
    super(type, options);
    if (options && options.detail) {
      this.detail = options.detail;
    }
  }
}
// The listeners for an EventTarget are maintained as a linked list.
// Unfortunately, the way EventTarget is defined, listeners are accounted
// using the tuple [handler,capture], and even if we don't actually make
// use of capture or bubbling, in order to be spec compliant we have to
// take on the additional complexity of supporting it. Fortunately, using
// the linked list makes dispatching faster, even if adding/removing is
// slower.
class Listener {
  constructor(previous, listener, once, capture, passive, isNodeStyleListener) {
    this.next = undefined;
    if (previous !== undefined)
      previous.next = this;
    this.previous = previous;
    this[kEventListener] = listener;
    // TODO(benjamingr) these 4 can be 'flags' to save 3 slots
    this.once = once;
    this.capture = capture;
    this.passive = passive;
    this.isNodeStyleListener = isNodeStyleListener;
    this.removed = false;

    this.callback =
      typeof listener === 'function' ?
        listener :
        listener.handleEvent.bind(listener);
  }

  same(listener, capture) {
    return this[kEventListener] === listener && this.capture === capture;
  }

  remove() {
    if (this.previous !== undefined)
      this.previous.next = this.next;
    if (this.next !== undefined)
      this.next.previous = this.previous;
    this.removed = true;
  }
}

function initEventTarget(self) {
  self[kEvents] = new SafeMap();
  self[kMaxEventTargetListeners] = EventEmitter.defaultMaxListeners;
  self[kMaxEventTargetListenersWarned] = false;
}

class EventTarget {
  // Used in checking whether an object is an EventTarget. This is a well-known
  // symbol as EventTarget may be used cross-realm.
  // Ref: https://github.com/nodejs/node/pull/33661
  static [kIsEventTarget] = true;

  constructor() {
    initEventTarget(this);
  }

  [kNewListener](size, type, listener, once, capture, passive) {
    if (this[kMaxEventTargetListeners] > 0 &&
        size > this[kMaxEventTargetListeners] &&
        !this[kMaxEventTargetListenersWarned]) {
      this[kMaxEventTargetListenersWarned] = true;
      // No error code for this since it is a Warning
      // eslint-disable-next-line no-restricted-syntax
      const w = new Error('Possible EventTarget memory leak detected. ' +
                          `${size} ${type} listeners ` +
                          `added to ${inspect(this, { depth: -1 })}. Use ` +
                          'events.setMaxListeners() to increase limit');
      w.name = 'MaxListenersExceededWarning';
      w.target = this;
      w.type = type;
      w.count = size;
      process.emitWarning(w);
    }
  }
  [kRemoveListener](size, type, listener, capture) {}

  addEventListener(type, listener, options = {}) {
    if (arguments.length < 2)
      throw new ERR_MISSING_ARGS('type', 'listener');

    // We validateOptions before the shouldAddListeners check because the spec
    // requires us to hit getters.
    const {
      once,
      capture,
      passive,
      signal,
      isNodeStyleListener
    } = validateEventListenerOptions(options);

    if (!shouldAddListener(listener)) {
      // The DOM silently allows passing undefined as a second argument
      // No error code for this since it is a Warning
      // eslint-disable-next-line no-restricted-syntax
      const w = new Error(`addEventListener called with ${listener}` +
                          ' which has no effect.');
      w.name = 'AddEventListenerArgumentTypeWarning';
      w.target = this;
      w.type = type;
      process.emitWarning(w);
      return;
    }
    type = String(type);

    if (signal) {
      if (signal.aborted) {
        return false;
      }
      // TODO(benjamingr) make this weak somehow? ideally the signal would
      // not prevent the event target from GC.
      signal.addEventListener('abort', () => {
        this.removeEventListener(type, listener, options);
      }, { once: true });
    }

    let root = this[kEvents].get(type);

    if (root === undefined) {
      root = { size: 1, next: undefined };
      // This is the first handler in our linked list.
      new Listener(root, listener, once, capture, passive, isNodeStyleListener);
      this[kNewListener](root.size, type, listener, once, capture, passive);
      this[kEvents].set(type, root);
      return;
    }

    let handler = root.next;
    let previous = root;

    // We have to walk the linked list to see if we have a match
    while (handler !== undefined && !handler.same(listener, capture)) {
      previous = handler;
      handler = handler.next;
    }

    if (handler !== undefined) { // Duplicate! Ignore
      return;
    }

    new Listener(previous, listener, once, capture, passive,
                 isNodeStyleListener);
    root.size++;
    this[kNewListener](root.size, type, listener, once, capture, passive);
  }

  removeEventListener(type, listener, options = {}) {
    if (!shouldAddListener(listener))
      return;

    type = String(type);
    // TODO(@jasnell): If it's determined this cannot be backported
    // to 12.x, then this can be simplified to:
    //   const capture = Boolean(options?.capture);
    const capture = options != null && options.capture === true;

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
    if (!(event instanceof Event))
      throw new ERR_INVALID_ARG_TYPE('event', 'Event', event);

    if (!isEventTarget(this))
      throw new ERR_INVALID_THIS('EventTarget');

    if (event[kTarget] !== null)
      throw new ERR_EVENT_RECURSION(event.type);

    this[kHybridDispatch](event, event.type, event);

    return event.defaultPrevented !== true;
  }

  [kHybridDispatch](nodeValue, type, event) {
    const createEvent = () => {
      if (event === undefined) {
        event = this[kCreateEvent](nodeValue, type);
        event[kTarget] = this;
      }
      return event;
    };
    if (event !== undefined)
      event[kTarget] = this;

    const root = this[kEvents].get(type);
    if (root === undefined || root.next === undefined)
      return true;

    let handler = root.next;
    let next;

    while (handler !== undefined &&
           (handler.passive || event?.[kStop] !== true)) {
      // Cache the next item in case this iteration removes the current one
      next = handler.next;

      if (handler.removed) {
        // Deal with the case an event is removed while event handlers are
        // Being processed (removeEventListener called from a listener)
        handler = next;
        continue;
      }
      if (handler.once) {
        handler.remove();
        root.size--;
        const { listener, capture } = handler;
        this[kRemoveListener](root.size, type, listener, capture);
      }

      try {
        let arg;
        if (handler.isNodeStyleListener) {
          arg = nodeValue;
        } else {
          arg = createEvent();
        }
        const result = handler.callback.call(this, arg);
        if (result !== undefined && result !== null)
          addCatch(this, result, createEvent());
      } catch (err) {
        emitUnhandledRejectionOrErr(this, err, createEvent());
      }

      handler = next;
    }

    if (event !== undefined)
      event[kTarget] = undefined;
  }

  [kCreateEvent](nodeValue, type) {
    return new NodeCustomEvent(type, { detail: nodeValue });
  }
  [customInspectSymbol](depth, options) {
    const name = this.constructor.name;
    if (depth < 0)
      return name;

    const opts = ObjectAssign({}, options, {
      depth: NumberIsInteger(options.depth) ? options.depth - 1 : options.depth
    });

    return `${name} ${inspect({}, opts)}`;
  }
}

ObjectDefineProperties(EventTarget.prototype, {
  addEventListener: { enumerable: true },
  removeEventListener: { enumerable: true },
  dispatchEvent: { enumerable: true }
});
ObjectDefineProperty(EventTarget.prototype, SymbolToStringTag, {
  writable: false,
  enumerable: false,
  configurable: true,
  value: 'EventTarget',
});

function initNodeEventTarget(self) {
  initEventTarget(self);
}

class NodeEventTarget extends EventTarget {
  static defaultMaxListeners = 10;

  constructor() {
    super();
    initNodeEventTarget(this);
  }

  setMaxListeners(n) {
    EventEmitter.setMaxListeners(n, this);
  }

  getMaxListeners() {
    return this[kMaxEventTargetListeners];
  }

  eventNames() {
    return ArrayFrom(this[kEvents].keys());
  }

  listenerCount(type) {
    const root = this[kEvents].get(String(type));
    return root !== undefined ? root.size : 0;
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
    this.addEventListener(type, listener, { [kIsNodeStyleListener]: true });
    return this;
  }

  addListener(type, listener) {
    this.addEventListener(type, listener, { [kIsNodeStyleListener]: true });
    return this;
  }
  emit(type, arg) {
    if (typeof type !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('type', 'string', type);
    }
    const hadListeners = this.listenerCount(type) > 0;
    this[kHybridDispatch](arg, type);
    return hadListeners;
  }

  once(type, listener) {
    this.addEventListener(type, listener,
                          { once: true, [kIsNodeStyleListener]: true });
    return this;
  }

  removeAllListeners(type) {
    if (type !== undefined) {
      this[kEvents].delete(String(type));
    } else {
      this[kEvents].clear();
    }

    return this;
  }
}

ObjectDefineProperties(NodeEventTarget.prototype, {
  setMaxListeners: { enumerable: true },
  getMaxListeners: { enumerable: true },
  eventNames: { enumerable: true },
  listenerCount: { enumerable: true },
  off: { enumerable: true },
  removeListener: { enumerable: true },
  on: { enumerable: true },
  addListener: { enumerable: true },
  once: { enumerable: true },
  emit: { enumerable: true },
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
    signal: options.signal,
    isNodeStyleListener: Boolean(options[kIsNodeStyleListener])
  };
}

// Test whether the argument is an event object. This is far from a fool-proof
// test, for example this input will result in a false positive:
// > isEventTarget({ constructor: EventTarget })
// It stands in its current implementation as a compromise.
// Ref: https://github.com/nodejs/node/pull/33661
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

function makeEventHandler(handler) {
  // Event handlers are dispatched in the order they were first set
  // See https://github.com/nodejs/node/pull/35949#issuecomment-722496598
  function eventHandler(...args) {
    if (typeof eventHandler.handler !== 'function') {
      return;
    }
    return ReflectApply(eventHandler.handler, this, args);
  }
  eventHandler.handler = handler;
  return eventHandler;
}

function defineEventHandler(emitter, name) {
  // 8.1.5.1 Event handlers - basically `on[eventName]` attributes
  ObjectDefineProperty(emitter, `on${name}`, {
    get() {
      return this[kHandlers]?.get(name)?.handler;
    },
    set(value) {
      if (!this[kHandlers]) {
        this[kHandlers] = new SafeMap();
      }
      let wrappedHandler = this[kHandlers]?.get(name);
      if (wrappedHandler) {
        if (typeof wrappedHandler.handler === 'function') {
          this[kEvents].get(name).size--;
          const size = this[kEvents].get(name).size;
          this[kRemoveListener](size, name, wrappedHandler.handler, false);
        }
        wrappedHandler.handler = value;
        if (typeof wrappedHandler.handler === 'function') {
          this[kEvents].get(name).size++;
          const size = this[kEvents].get(name).size;
          this[kNewListener](size, name, value, false, false, false);
        }
      } else {
        wrappedHandler = makeEventHandler(value);
        this.addEventListener(name, wrappedHandler);
      }
      this[kHandlers].set(name, wrappedHandler);
    },
    configurable: true,
    enumerable: true
  });
}

const EventEmitterMixin = (Superclass) => {
  class MixedEventEmitter extends Superclass {
    constructor(...args) {
      super(...args);
      FunctionPrototypeCall(EventEmitter, this);
    }
  }
  const protoProps = ObjectGetOwnPropertyDescriptors(EventEmitter.prototype);
  delete protoProps.constructor;
  ObjectDefineProperties(MixedEventEmitter.prototype, protoProps);
  return MixedEventEmitter;
};

module.exports = {
  Event,
  EventEmitterMixin,
  EventTarget,
  NodeEventTarget,
  defineEventHandler,
  initEventTarget,
  initNodeEventTarget,
  kCreateEvent,
  kNewListener,
  kTrustEvent,
  kRemoveListener,
  kEvents,
  isEventTarget,
};

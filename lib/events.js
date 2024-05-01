// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const {
  ArrayPrototypePop,
  ArrayPrototypePush,
  ArrayPrototypeUnshift,
  Boolean,
  Error,
  FunctionPrototypeCall,
  NumberMAX_SAFE_INTEGER,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectEntries,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  Promise,
  PromiseReject,
  PromiseResolve,
  ReflectApply,
  ReflectOwnKeys,
  Symbol,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolFor,
} = primordials;
const kRejection = SymbolFor('nodejs.rejection');

const { kEmptyObject } = require('internal/util');


let FixedQueue;
let kFirstEventParam;
let kResistStopPropagation;

const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  validateInteger,
  validateAbortSignal,
  validateBoolean,
  validateFunction,
  validateNumber,
  validateObject,
  validateString,
} = require('internal/validators');
const { addAbortListener } = require('internal/events/abort_listener');
const {
  kInitialFastPath,
  kCapture,
  kErrorMonitor,
  kShapeMode,
  kMaxEventTargetListeners,
  kMaxEventTargetListenersWarned,
  kWatermarkData,
  kImpl,
  kIsFastPath,
  kSwitchToSlowPath,
  kEvents,
} = require('internal/events/symbols');
const {
  arrayClone,
} = require('internal/events/shared_internal_event_emitter');
const { SlowEventEmitter } = require('internal/events/slow_event_emitter');
const { FastEventEmitter } = require('internal/events/fast_event_emitter');


let EventEmitterAsyncResource;
// The EventEmitterAsyncResource has to be initialized lazily because event.js
// is loaded so early in the bootstrap process, before async_hooks is available.
//
// This implementation was adapted straight from addaleax's
// eventemitter-asyncresource MIT-licensed userland module.
// https://github.com/addaleax/eventemitter-asyncresource
function lazyEventEmitterAsyncResource() {
  if (EventEmitterAsyncResource === undefined) {
    const {
      AsyncResource,
    } = require('async_hooks');

    const kEventEmitter = Symbol('kEventEmitter');
    const kAsyncResource = Symbol('kAsyncResource');
    class EventEmitterReferencingAsyncResource extends AsyncResource {
      /**
       * @param {EventEmitter} ee
       * @param {string} [type]
       * @param {{
       *   triggerAsyncId?: number,
       *   requireManualDestroy?: boolean,
       * }} [options]
       */
      constructor(ee, type, options) {
        super(type, options);
        this[kEventEmitter] = ee;
      }

      /**
       * @type {EventEmitter}
       */
      get eventEmitter() {
        if (this[kEventEmitter] === undefined)
          throw new ERR_INVALID_THIS('EventEmitterReferencingAsyncResource');
        return this[kEventEmitter];
      }
    }

    EventEmitterAsyncResource =
      class EventEmitterAsyncResource extends EventEmitter {
        /**
         * @param {{
         *   name?: string,
         *   triggerAsyncId?: number,
         *   requireManualDestroy?: boolean,
         * }} [options]
         */
        constructor(options = undefined) {
          let name;
          if (typeof options === 'string') {
            name = options;
            options = undefined;
          } else {
            if (new.target === EventEmitterAsyncResource) {
              validateString(options?.name, 'options.name');
            }
            name = options?.name || new.target.name;
          }
          super(options);

          this[kAsyncResource] =
            new EventEmitterReferencingAsyncResource(this, name, options);
        }

        /**
         * @param {symbol,string} event
         * @param  {...any} args
         * @returns {boolean}
         */
        emit(event, ...args) {
          if (this[kAsyncResource] === undefined)
            throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
          const { asyncResource } = this;
          ArrayPrototypeUnshift(args, super.emit, this, event);
          return ReflectApply(asyncResource.runInAsyncScope, asyncResource,
                              args);
        }

        /**
         * @returns {void}
         */
        emitDestroy() {
          if (this[kAsyncResource] === undefined)
            throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
          this.asyncResource.emitDestroy();
        }

        /**
         * @type {number}
         */
        get asyncId() {
          if (this[kAsyncResource] === undefined)
            throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
          return this.asyncResource.asyncId();
        }

        /**
         * @type {number}
         */
        get triggerAsyncId() {
          if (this[kAsyncResource] === undefined)
            throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
          return this.asyncResource.triggerAsyncId();
        }

        /**
         * @type {EventEmitterReferencingAsyncResource}
         */
        get asyncResource() {
          if (this[kAsyncResource] === undefined)
            throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
          return this[kAsyncResource];
        }
      };
  }
  return EventEmitterAsyncResource;
}

/**
 * Creates a new `EventEmitter` instance.
 * @param {{ captureRejections?: boolean; }} [opts]
 * @constructs {EventEmitter}
 */
function EventEmitter(opts) {
  EventEmitter.init.call(this, opts);
}
module.exports = EventEmitter;
module.exports.addAbortListener = addAbortListener;
module.exports.once = once;
module.exports.on = on;
module.exports.getEventListeners = getEventListeners;
module.exports.getMaxListeners = getMaxListeners;
// Backwards-compat with node 0.10.x
EventEmitter.EventEmitter = EventEmitter;

EventEmitter.usingDomains = false;

EventEmitter.captureRejectionSymbol = kRejection;
ObjectDefineProperty(EventEmitter, 'captureRejections', {
  __proto__: null,
  get() {
    return EventEmitter.prototype[kCapture];
  },
  set(value) {
    validateBoolean(value, 'EventEmitter.captureRejections');

    EventEmitter.prototype[kCapture] = value;
  },
  enumerable: true,
});

ObjectDefineProperty(EventEmitter, 'EventEmitterAsyncResource', {
  __proto__: null,
  enumerable: true,
  get: lazyEventEmitterAsyncResource,
  set: undefined,
  configurable: true,
});

EventEmitter.errorMonitor = kErrorMonitor;

// The default for captureRejections is false
ObjectDefineProperty(EventEmitter.prototype, kCapture, {
  __proto__: null,
  value: false,
  writable: true,
  enumerable: false,
});

function isEventUnsupportedForFastPath(type) {
  // Not supporting newListener and removeListener events in fast path
  // as they can add new listeners in the middle of the process
  // and also not supporting errorMonitor event as it has special handling
  return type === 'newListener' || type === 'removeListener' || type === kErrorMonitor;
}

ObjectDefineProperties(EventEmitter.prototype, {
  [kImpl]: {
    __proto__: null,
    value: undefined,
    enumerable: false,
    configurable: false,
    writable: true,
  },
  [kIsFastPath]: {
    __proto__: null,
    value: undefined,
    enumerable: false,
    configurable: false,
    writable: true,
  },
  [kInitialFastPath]: {
    __proto__: null,
    value: undefined,
    enumerable: false,
    configurable: false,
    writable: true,
  },
  _events: {
    __proto__: null,
    enumerable: true,

    get: function() {
      // TODO - remove optional chaining
      return this[kImpl]?._events;
    },
    set: function(arg) {

      if (this[kImpl] === undefined && this[kInitialFastPath] === true) {
        // TODO(rluvaton): remove this eslint ignore
        // eslint-disable-next-line no-use-before-define
        this[kImpl] = new FastEventEmitter(this, arg);
        this[kIsFastPath] = true;
        this[kInitialFastPath] = undefined;
        return;
      }

      if (this[kImpl] === undefined) {

        // TODO(rluvaton): find a better way and also support array with length 1?
        // TODO(rluvaton): if have newListener and removeListener events, switch to slow path

        const shouldBeFastPath = arg === undefined ||
          ObjectEntries(arg).some(({ 0: key, 1: value }) =>
            isEventUnsupportedForFastPath(key) || (typeof value !== 'undefined' && typeof value !== 'function'));

        // TODO(rluvaton): remove this eslint ignore
        // eslint-disable-next-line no-use-before-define
        this[kImpl] = shouldBeFastPath ? new FastEventEmitter(this, arg) : new SlowEventEmitter(this, arg);
        this[kIsFastPath] = shouldBeFastPath;
        return;
      }

      // TODO - might need to change to slow path
      // TODO - deprecate this
      this[kImpl]._events = arg;
    },
  },
  _eventsCount: {
    __proto__: null,

    enumerable: true,

    get: function() {
      return this[kImpl]?._eventsCount;
    },
    set: function(arg) {
      // TODO - remove optional chaining
      if (!this[kImpl]) {
        // TODO - don't use slow by default here

        // TODO(rluvaton): remove this eslint ignore
        // eslint-disable-next-line no-use-before-define
        this[kImpl] = new SlowEventEmitter(this);
        this[kIsFastPath] = false;
        // return;
      }
      // TODO - deprecate this
      this[kImpl]._eventsCount = arg;
    },
  },
});

EventEmitter.prototype._maxListeners = undefined;
EventEmitter.prototype[kInitialFastPath] = undefined;

// By default EventEmitters will print a warning if more than 10 listeners are
// added to it. This is a useful default which helps finding memory leaks.
let defaultMaxListeners = 10;
let isEventTarget;

function checkListener(listener) {
  validateFunction(listener, 'listener');
}

ObjectDefineProperty(EventEmitter, 'defaultMaxListeners', {
  __proto__: null,
  enumerable: true,
  get: function() {
    return defaultMaxListeners;
  },
  set: function(arg) {
    validateNumber(arg, 'defaultMaxListeners', 0);
    defaultMaxListeners = arg;
  },
});

ObjectDefineProperties(EventEmitter, {
  kMaxEventTargetListeners: {
    __proto__: null,
    value: kMaxEventTargetListeners,
    enumerable: false,
    configurable: false,
    writable: false,
  },
  kMaxEventTargetListenersWarned: {
    __proto__: null,
    value: kMaxEventTargetListenersWarned,
    enumerable: false,
    configurable: false,
    writable: false,
  },
});

/**
 * Sets the max listeners.
 * @param {number} n
 * @param {EventTarget[] | EventEmitter[]} [eventTargets]
 * @returns {void}
 */
EventEmitter.setMaxListeners =
  function(n = defaultMaxListeners, ...eventTargets) {
    validateNumber(n, 'setMaxListeners', 0);
    if (eventTargets.length === 0) {
      defaultMaxListeners = n;
    } else {
      if (isEventTarget === undefined)
        isEventTarget = require('internal/event_target').isEventTarget;

      for (let i = 0; i < eventTargets.length; i++) {
        const target = eventTargets[i];
        if (isEventTarget(target)) {
          target[kMaxEventTargetListeners] = n;
          target[kMaxEventTargetListenersWarned] = false;
        } else if (typeof target.setMaxListeners === 'function') {
          target.setMaxListeners(n);
        } else {
          throw new ERR_INVALID_ARG_TYPE(
            'eventTargets',
            ['EventEmitter', 'EventTarget'],
            target);
        }
      }
    }
  };

// If you're updating this function definition, please also update any
// re-definitions, such as the one in the Domain module (lib/domain.js).
EventEmitter.init = function(opts) {
  // TODO(rluvaton): remove this eslint ignore
  // eslint-disable-next-line no-use-before-define
  this[kImpl] ??= new FastEventEmitter(this, opts?.[kEvents]);
  this[kIsFastPath] ??= true;

  const thisEvents = this._events;


  // TODO - update this
  if (thisEvents === undefined ||
      thisEvents === ObjectGetPrototypeOf(this)._events) {
    // In fast path we don't want to have __proto__: null as it will cause the object to be in dictionary mode
    // and will slow down the access to the properties by a lot (around 2x)
    this[kImpl]._events = this[kIsFastPath] ? {} : { __proto__: null };
    this[kImpl]._eventsCount = 0;
    this[kImpl][kShapeMode] = false;
  } else {
    this[kImpl][kShapeMode] = true;
  }

  this._maxListeners = this._maxListeners || undefined;


  if (opts?.captureRejections) {
    validateBoolean(opts.captureRejections, 'options.captureRejections');
    this[kCapture] = Boolean(opts.captureRejections);
  } else {
    // Assigning the kCapture property directly saves an expensive
    // prototype lookup in a very sensitive hot path.
    this[kCapture] = EventEmitter.prototype[kCapture];
  }
};

EventEmitter.prototype[kSwitchToSlowPath] = function() {
  if (this[kIsFastPath] === false) {
    return;
  }

  if (this[kIsFastPath] === true) {
    this[kIsFastPath] = false;

    // TODO(rluvaton): remove this eslint ignore
    // eslint-disable-next-line no-use-before-define
    this[kImpl] = SlowEventEmitter.fromFastEventEmitter(this[kImpl]);
    return;
  }

  // TODO(rluvaton): remove this eslint ignore
  // eslint-disable-next-line no-use-before-define
  this[kImpl] = new SlowEventEmitter(this);
  this[kIsFastPath] = false;

};

/**
 * Increases the max listeners of the event emitter.
 * @param {number} n
 * @returns {EventEmitter}
 */
EventEmitter.prototype.setMaxListeners = function setMaxListeners(n) {
  validateNumber(n, 'setMaxListeners', 0);
  this._maxListeners = n;
  return this;
};

function _getMaxListeners(that) {
  if (that._maxListeners === undefined)
    return EventEmitter.defaultMaxListeners;
  return that._maxListeners;
}

/**
 * Returns the current max listener value for the event emitter.
 * @returns {number}
 */
EventEmitter.prototype.getMaxListeners = function getMaxListeners() {
  return _getMaxListeners(this);
};


/**
 * Synchronously calls each of the listeners registered
 * for the event.
 * @param {string | symbol} type
 * @param {...any} [args]
 * @returns {boolean}
 */
EventEmitter.prototype.emit = function emit(type, ...args) {
  return this[kImpl].emit(type, ...args);
};

function _addListener(target, type, listener, prepend) {
  let m;
  let events;
  let existing;

  checkListener(listener);

  events = target._events;
  if (events === undefined) {
    events = target._events = { __proto__: null };
    target._eventsCount = 0;
  } else {
    // To avoid recursion in the case that type === "newListener"! Before
    // adding it to the listeners, first emit "newListener".
    if (events.newListener !== undefined) {
      target.emit('newListener', type,
                  listener.listener ?? listener);

      // Re-assign `events` because a newListener handler could have caused the
      // this._events to be assigned to a new object
      events = target._events;
    }
    existing = events[type];
  }

  if (existing === undefined) {
    // Optimize the case of one listener. Don't need the extra array object.
    events[type] = listener;
    ++target._eventsCount;
  } else {
    if (typeof existing === 'function') {
      // Adding the second element, need to change to array.
      existing = events[type] =
        prepend ? [listener, existing] : [existing, listener];
      // If we've already got an array, just append.
    } else if (prepend) {
      existing.unshift(listener);
    } else {
      existing.push(listener);
    }

    // Check for listener leak
    m = _getMaxListeners(target);
    if (m > 0 && existing.length > m && !existing.warned) {
      existing.warned = true;
      // No error code for this since it is a Warning
      const w = genericNodeError(
        `Possible EventEmitter memory leak detected. ${existing.length} ${String(type)} listeners ` +
        `added to ${inspect(target, { depth: -1 })}. MaxListeners is ${m}. Use emitter.setMaxListeners() to increase limit`,
        { name: 'MaxListenersExceededWarning', emitter: target, type: type, count: existing.length });
      process.emitWarning(w);
    }
  }

  return target;
}

/**
 * Adds a listener to the event emitter.
 * @param {string | symbol} type
 * @param {Function} listener
 * @returns {EventEmitter}
 */
EventEmitter.prototype.addListener = function addListener(type, listener) {
  checkListener(listener);

  // This can happen in TLSSocket, where we listen for close event before
  // the EventEmitter was initiated
  // TODO(rluvaton): check how it worked before?
  if (!this[kImpl]) {
    EventEmitter.init.apply(this);
  }

  // If the listener is already added,
  // switch to slow path as the fast path optimized for single listener for each event
  if (this[kIsFastPath] && (isEventUnsupportedForFastPath(type) || this[kImpl].isListenerAlreadyExists(type))) {
    this[kSwitchToSlowPath]();
  }

  return this[kImpl].addListener(type, listener);
};

EventEmitter.prototype.on = EventEmitter.prototype.addListener;

/**
 * Adds the `listener` function to the beginning of
 * the listeners array.
 * @param {string | symbol} type
 * @param {Function} listener
 * @returns {EventEmitter}
 */
EventEmitter.prototype.prependListener =
    function prependListener(type, listener) {
      checkListener(listener);

      // This can happen in TLSSocket, where we listen for close event before
      // the EventEmitter was initiated
      // TODO(rluvaton): check how it worked before?
      if (!this[kImpl]) {
        EventEmitter.init.apply(this);
      }

      // If the listener is already added,
      // switch to slow path as the fast path optimized for single listener for each event
      if (this[kIsFastPath] && (isEventUnsupportedForFastPath(type) || this[kImpl].isListenerAlreadyExists(type))) {
        this[kSwitchToSlowPath]();
      }

      return this[kImpl].addListener(type, listener, true);
    };

function onceWrapper() {
  if (!this.fired) {
    this.target.removeListener(this.type, this.wrapFn);
    this.fired = true;
    if (arguments.length === 0)
      return this.listener.call(this.target);
    return this.listener.apply(this.target, arguments);
  }
}

function _onceWrap(target, type, listener) {
  const state = { fired: false, wrapFn: undefined, target, type, listener };
  const wrapped = onceWrapper.bind(state);
  wrapped.listener = listener;
  state.wrapFn = wrapped;
  return wrapped;
}

/**
 * Adds a one-time `listener` function to the event emitter.
 * @param {string | symbol} type
 * @param {Function} listener
 * @returns {EventEmitter}
 */
EventEmitter.prototype.once = function once(type, listener) {
  checkListener(listener);

  this.on(type, _onceWrap(this, type, listener));
  return this;
};

/**
 * Adds a one-time `listener` function to the beginning of
 * the listeners array.
 * @param {string | symbol} type
 * @param {Function} listener
 * @returns {EventEmitter}
 */
EventEmitter.prototype.prependOnceListener =
    function prependOnceListener(type, listener) {
      checkListener(listener);

      this.prependListener(type, _onceWrap(this, type, listener));
      return this;
    };

/**
 * Removes the specified `listener` from the listeners array.
 * @param {string | symbol} type
 * @param {Function} listener
 * @returns {EventEmitter}
 */
EventEmitter.prototype.removeListener =
    function removeListener(type, listener) {
      checkListener(listener);

      this[kImpl].removeListener(type, listener);

      return this;
    };

EventEmitter.prototype.off = EventEmitter.prototype.removeListener;

/**
 * Removes all listeners from the event emitter. (Only
 * removes listeners for a specific event name if specified
 * as `type`).
 * @param {string | symbol} [type]
 * @returns {EventEmitter}
 */
EventEmitter.prototype.removeAllListeners =
    function removeAllListeners(type) {
      this[kImpl].removeAllListeners(type);
      return this;
    };

function _listeners(target, type, unwrap) {
  const events = target._events;

  if (events === undefined)
    return [];

  const evlistener = events[type];
  if (evlistener === undefined)
    return [];

  if (typeof evlistener === 'function')
    return unwrap ? [evlistener.listener || evlistener] : [evlistener];

  return unwrap ?
    unwrapListeners(evlistener) : arrayClone(evlistener);
}

/**
 * Returns a copy of the array of listeners for the event name
 * specified as `type`.
 * @param {string | symbol} type
 * @returns {Function[]}
 */
EventEmitter.prototype.listeners = function listeners(type) {
  return _listeners(this, type, true);
};

/**
 * Returns a copy of the array of listeners and wrappers for
 * the event name specified as `type`.
 * @param {string | symbol} type
 * @returns {Function[]}
 */
EventEmitter.prototype.rawListeners = function rawListeners(type) {
  return _listeners(this, type, false);
};

/**
 * Returns the number of listeners listening to the event name
 * specified as `type`.
 * @deprecated since v3.2.0
 * @param {EventEmitter} emitter
 * @param {string | symbol} type
 * @returns {number}
 */
EventEmitter.listenerCount = function(emitter, type) {
  if (typeof emitter.listenerCount === 'function') {
    return emitter.listenerCount(type);
  }
  return FunctionPrototypeCall(listenerCount, emitter, type);
};

EventEmitter.prototype.listenerCount = listenerCount;

/**
 * Returns the number of listeners listening to event name
 * specified as `type`.
 * @param {string | symbol} type
 * @param {Function} listener
 * @returns {number}
 */
function listenerCount(type, listener) {
  const events = this._events;

  if (events !== undefined) {
    const evlistener = events[type];

    if (typeof evlistener === 'function') {
      if (listener != null) {
        return listener === evlistener || listener === evlistener.listener ? 1 : 0;
      }

      return 1;
    } else if (evlistener !== undefined) {
      if (listener != null) {
        let matching = 0;

        for (let i = 0, l = evlistener.length; i < l; i++) {
          if (evlistener[i] === listener || evlistener[i].listener === listener) {
            matching++;
          }
        }

        return matching;
      }

      return evlistener.length;
    }
  }

  return 0;
}

/**
 * Returns an array listing the events for which
 * the emitter has registered listeners.
 * @returns {any[]}
 */
EventEmitter.prototype.eventNames = function eventNames() {
  return this._eventsCount > 0 ? ReflectOwnKeys(this._events) : [];
};

function unwrapListeners(arr) {
  const ret = arrayClone(arr);
  for (let i = 0; i < ret.length; ++i) {
    const orig = ret[i].listener;
    if (typeof orig === 'function')
      ret[i] = orig;
  }
  return ret;
}

/**
 * Returns a copy of the array of listeners for the event name
 * specified as `type`.
 * @param {EventEmitter | EventTarget} emitterOrTarget
 * @param {string | symbol} type
 * @returns {Function[]}
 */
function getEventListeners(emitterOrTarget, type) {
  // First check if EventEmitter
  if (typeof emitterOrTarget.listeners === 'function') {
    return emitterOrTarget.listeners(type);
  }
  // Require event target lazily to avoid always loading it
  const { isEventTarget, kEvents } = require('internal/event_target');
  if (isEventTarget(emitterOrTarget)) {
    const root = emitterOrTarget[kEvents].get(type);
    const listeners = [];
    let handler = root?.next;
    while (handler?.listener !== undefined) {
      const listener = handler.listener?.deref ?
        handler.listener.deref() : handler.listener;
      listeners.push(listener);
      handler = handler.next;
    }
    return listeners;
  }
  throw new ERR_INVALID_ARG_TYPE('emitter',
                                 ['EventEmitter', 'EventTarget'],
                                 emitterOrTarget);
}

/**
 * Returns the max listeners set.
 * @param {EventEmitter | EventTarget} emitterOrTarget
 * @returns {number}
 */
function getMaxListeners(emitterOrTarget) {
  if (typeof emitterOrTarget?.getMaxListeners === 'function') {
    return _getMaxListeners(emitterOrTarget);
  } else if (emitterOrTarget?.[kMaxEventTargetListeners]) {
    return emitterOrTarget[kMaxEventTargetListeners];
  }

  throw new ERR_INVALID_ARG_TYPE('emitter',
                                 ['EventEmitter', 'EventTarget'],
                                 emitterOrTarget);
}

/**
 * Creates a `Promise` that is fulfilled when the emitter
 * emits the given event.
 * @param {EventEmitter} emitter
 * @param {string} name
 * @param {{ signal: AbortSignal; }} [options]
 * @returns {Promise}
 */
async function once(emitter, name, options = kEmptyObject) {
  validateObject(options, 'options');
  const signal = options?.signal;
  validateAbortSignal(signal, 'options.signal');
  if (signal?.aborted)
    throw new AbortError(undefined, { cause: signal?.reason });
  return new Promise((resolve, reject) => {
    const errorListener = (err) => {
      emitter.removeListener(name, resolver);
      if (signal != null) {
        eventTargetAgnosticRemoveListener(signal, 'abort', abortListener);
      }
      reject(err);
    };
    const resolver = (...args) => {
      if (typeof emitter.removeListener === 'function') {
        emitter.removeListener('error', errorListener);
      }
      if (signal != null) {
        eventTargetAgnosticRemoveListener(signal, 'abort', abortListener);
      }
      resolve(args);
    };

    kResistStopPropagation ??= require('internal/event_target').kResistStopPropagation;
    const opts = { __proto__: null, once: true, [kResistStopPropagation]: true };
    eventTargetAgnosticAddListener(emitter, name, resolver, opts);
    if (name !== 'error' && typeof emitter.once === 'function') {
      // EventTarget does not have `error` event semantics like Node
      // EventEmitters, we listen to `error` events only on EventEmitters.
      emitter.once('error', errorListener);
    }
    function abortListener() {
      eventTargetAgnosticRemoveListener(emitter, name, resolver);
      eventTargetAgnosticRemoveListener(emitter, 'error', errorListener);
      reject(new AbortError(undefined, { cause: signal?.reason }));
    }
    if (signal != null) {
      eventTargetAgnosticAddListener(
        signal, 'abort', abortListener, { __proto__: null, once: true, [kResistStopPropagation]: true });
    }
  });
}

const AsyncIteratorPrototype = ObjectGetPrototypeOf(
  ObjectGetPrototypeOf(async function* () {}).prototype);

function createIterResult(value, done) {
  return { value, done };
}

function eventTargetAgnosticRemoveListener(emitter, name, listener, flags) {
  if (typeof emitter.removeListener === 'function') {
    emitter.removeListener(name, listener);
  } else if (typeof emitter.removeEventListener === 'function') {
    emitter.removeEventListener(name, listener, flags);
  } else {
    throw new ERR_INVALID_ARG_TYPE('emitter', 'EventEmitter', emitter);
  }
}

function eventTargetAgnosticAddListener(emitter, name, listener, flags) {
  if (typeof emitter.on === 'function') {
    if (flags?.once) {
      emitter.once(name, listener);
    } else {
      emitter.on(name, listener);
    }
  } else if (typeof emitter.addEventListener === 'function') {
    emitter.addEventListener(name, listener, flags);
  } else {
    throw new ERR_INVALID_ARG_TYPE('emitter', 'EventEmitter', emitter);
  }
}

/**
 * Returns an `AsyncIterator` that iterates `event` events.
 * @param {EventEmitter} emitter
 * @param {string | symbol} event
 * @param {{
 *    signal: AbortSignal;
 *    close?: string[];
 *    highWaterMark?: number,
 *    lowWaterMark?: number
 *   }} [options]
 * @returns {AsyncIterator}
 */
function on(emitter, event, options = kEmptyObject) {
  // Parameters validation
  validateObject(options, 'options');
  const signal = options.signal;
  validateAbortSignal(signal, 'options.signal');
  if (signal?.aborted)
    throw new AbortError(undefined, { cause: signal?.reason });
  // Support both highWaterMark and highWatermark for backward compatibility
  const highWatermark = options.highWaterMark ?? options.highWatermark ?? NumberMAX_SAFE_INTEGER;
  validateInteger(highWatermark, 'options.highWaterMark', 1);
  // Support both lowWaterMark and lowWatermark for backward compatibility
  const lowWatermark = options.lowWaterMark ?? options.lowWatermark ?? 1;
  validateInteger(lowWatermark, 'options.lowWaterMark', 1);

  // Preparing controlling queues and variables
  FixedQueue ??= require('internal/fixed_queue');
  const unconsumedEvents = new FixedQueue();
  const unconsumedPromises = new FixedQueue();
  let paused = false;
  let error = null;
  let finished = false;
  let size = 0;

  const iterator = ObjectSetPrototypeOf({
    next() {
      // First, we consume all unread events
      if (size) {
        const value = unconsumedEvents.shift();
        size--;
        if (paused && size < lowWatermark) {
          emitter.resume();
          paused = false;
        }
        return PromiseResolve(createIterResult(value, false));
      }

      // Then we error, if an error happened
      // This happens one time if at all, because after 'error'
      // we stop listening
      if (error) {
        const p = PromiseReject(error);
        // Only the first element errors
        error = null;
        return p;
      }

      // If the iterator is finished, resolve to done
      if (finished) return closeHandler();

      // Wait until an event happens
      return new Promise(function(resolve, reject) {
        unconsumedPromises.push({ resolve, reject });
      });
    },

    return() {
      return closeHandler();
    },

    throw(err) {
      if (!err || !(err instanceof Error)) {
        throw new ERR_INVALID_ARG_TYPE('EventEmitter.AsyncIterator',
                                       'Error', err);
      }
      errorHandler(err);
    },
    [SymbolAsyncIterator]() {
      return this;
    },
    [kWatermarkData]: {
      /**
       * The current queue size
       */
      get size() {
        return size;
      },
      /**
       * The low watermark. The emitter is resumed every time size is lower than it
       */
      get low() {
        return lowWatermark;
      },
      /**
       * The high watermark. The emitter is paused every time size is higher than it
       */
      get high() {
        return highWatermark;
      },
      /**
       * It checks whether the emitter is paused by the watermark controller or not
       */
      get isPaused() {
        return paused;
      },
    },
  }, AsyncIteratorPrototype);

  // Adding event handlers
  const { addEventListener, removeAll } = listenersController();
  kFirstEventParam ??= require('internal/events/symbols').kFirstEventParam;
  addEventListener(emitter, event, options[kFirstEventParam] ? eventHandler : function(...args) {
    return eventHandler(args);
  });
  if (event !== 'error' && typeof emitter.on === 'function') {
    addEventListener(emitter, 'error', errorHandler);
  }
  const closeEvents = options?.close;
  if (closeEvents?.length) {
    for (let i = 0; i < closeEvents.length; i++) {
      addEventListener(emitter, closeEvents[i], closeHandler);
    }
  }

  const abortListenerDisposable = signal ? addAbortListener(signal, abortListener) : null;

  return iterator;

  function abortListener() {
    errorHandler(new AbortError(undefined, { cause: signal?.reason }));
  }

  function eventHandler(value) {
    if (unconsumedPromises.isEmpty()) {
      size++;
      if (!paused && size > highWatermark) {
        paused = true;
        emitter.pause();
      }
      unconsumedEvents.push(value);
    } else unconsumedPromises.shift().resolve(createIterResult(value, false));
  }

  function errorHandler(err) {
    if (unconsumedPromises.isEmpty()) error = err;
    else unconsumedPromises.shift().reject(err);

    closeHandler();
  }

  function closeHandler() {
    abortListenerDisposable?.[SymbolDispose]();
    removeAll();
    finished = true;
    const doneResult = createIterResult(undefined, true);
    while (!unconsumedPromises.isEmpty()) {
      unconsumedPromises.shift().resolve(doneResult);
    }

    return PromiseResolve(doneResult);
  }
}

function listenersController() {
  const listeners = [];

  return {
    addEventListener(emitter, event, handler, flags) {
      eventTargetAgnosticAddListener(emitter, event, handler, flags);
      ArrayPrototypePush(listeners, [emitter, event, handler, flags]);
    },
    removeAll() {
      while (listeners.length > 0) {
        ReflectApply(eventTargetAgnosticRemoveListener, undefined, ArrayPrototypePop(listeners));
      }
    },
  };
}

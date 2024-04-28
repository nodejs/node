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
  ArrayPrototypeJoin,
  ArrayPrototypePop,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  ArrayPrototypeUnshift,
  Boolean,
  Error,
  ErrorCaptureStackTrace,
  FunctionPrototypeBind,
  FunctionPrototypeCall,
  NumberMAX_SAFE_INTEGER,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  Promise,
  PromiseReject,
  PromiseResolve,
  ReflectApply,
  ReflectOwnKeys,
  String,
  StringPrototypeSplit,
  Symbol,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolFor,
} = primordials;
const kRejection = SymbolFor('nodejs.rejection');

const { kEmptyObject } = require('internal/util');

const {
  inspect,
  identicalSequenceRange,
} = require('internal/util/inspect');

let spliceOne;
let FixedQueue;
let kFirstEventParam;
let kResistStopPropagation;

const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_THIS,
    ERR_UNHANDLED_ERROR,
  },
  genericNodeError,
  kEnhanceStackBeforeInspector,
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

const kCapture = Symbol('kCapture');
const kErrorMonitor = Symbol('events.errorMonitor');
const kShapeMode = Symbol('shapeMode');
const kMaxEventTargetListeners = Symbol('events.maxEventTargetListeners');
const kMaxEventTargetListenersWarned =
  Symbol('events.maxEventTargetListenersWarned');
const kWatermarkData = SymbolFor('nodejs.watermarkData');

const kImpl = Symbol('kImpl');
const kCaptureValue = Symbol('kCaptureValue');
const kIsFastPath = Symbol('kIsFastPath');
const kSwitchToSlowPath = Symbol('kSwitchToSlowPath');
const kContinueAddEventListenerInSlowMode = Symbol('kContinueAddEventListenerInSlowMode');

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

function isEventUnsupportedForFastPath(type) {
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
    value: true,
    enumerable: false,
    configurable: false,
    writable: true,
  },
  [kCaptureValue]: {
    __proto__: null,
    value: false,
    enumerable: false,
    configurable: false,
    writable: true,
  },

  // The default for captureRejections is false
  [kCapture]: {
    __proto__: null,
    get() {
      return this[kCaptureValue];
    },
    set(value) {
      this[kCaptureValue] = value;

      if (value) {
        this[kSwitchToSlowPath]();
      }
    },
    enumerable: false,
  },
  _events: {
    __proto__: null,
    enumerable: true,

    get: function() {
      // TODO - remove optional chaining
      return this[kImpl]?._events;
    },
    set: function(arg) {
      // TODO - remove optional chaining
      if(!this[kImpl]) {
        // TODO(rluvaton): find a better way and also support array with length 1?
        // TODO(rluvaton): if have newListener and removeListener events, switch to slow path

        const shouldBeFastPath = arg === undefined || Object.entries(arg).every(([key, value]) => !isEventUnsupportedForFastPath(key) && typeof value === 'undefined' || typeof value === 'function');

        this[kImpl] = shouldBeFastPath ? new FastEventEmitter(this) : new SlowEventEmitter(this);
        this[kIsFastPath] = shouldBeFastPath;
        // return;
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
      if(!this[kImpl]) {
        // TODO - don't use slow by default here
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

  if (opts?.captureRejections) {
    validateBoolean(opts.captureRejections, 'options.captureRejections');
    this[kCapture] = Boolean(opts.captureRejections);
  } else {
    // Assigning the kCapture property directly saves an expensive
    // prototype lookup in a very sensitive hot path.
    this[kCapture] = EventEmitter.prototype[kCapture];
  }

  if(this[kCapture]) {
    if(this[kIsFastPath]) {
      this[kSwitchToSlowPath]();
    } else if(!this[kImpl]) {
      this[kImpl] = new SlowEventEmitter(this);
      this[kIsFastPath] = false;
    }
  }
  this[kImpl] ??= new FastEventEmitter(this, opts);
  this[kIsFastPath] ??= true;

  // TODO - update this
  if (this._events === undefined ||
      this._events === ObjectGetPrototypeOf(this)._events) {
    this._events = { __proto__: null };
    this._eventsCount = 0;
    this[kShapeMode] = false;
  } else {
    this[kShapeMode] = true;
  }

  this._maxListeners = this._maxListeners || undefined;
};

EventEmitter.prototype[kSwitchToSlowPath] = function() {
  if(!this[kIsFastPath]) {
    return;
  }

  this[kIsFastPath] = false;
  this[kImpl] = SlowEventEmitter.fromFastEventEmitter(this[kImpl]);
}

EventEmitter.prototype[kContinueAddEventListenerInSlowMode] = function(type, listener, prepend) {
  // TODO - this is in case while adding listener another listener added in the middle using the newListener event
  // TODO - maybe for simplicity we can just switch to slow mode if have newListener event
}

function addCatch(that, promise, type, args) {
  if (!that[kCapture]) {
    return;
  }

  // Handle Promises/A+ spec, then could be a getter
  // that throws on second use.
  try {
    const then = promise.then;

    if (typeof then === 'function') {
      then.call(promise, undefined, function(err) {
        // The callback is called with nextTick to avoid a follow-up
        // rejection from this promise.
        process.nextTick(emitUnhandledRejectionOrErr, that, err, type, args);
      });
    }
  } catch (err) {
    that.emit('error', err);
  }
}

function emitUnhandledRejectionOrErr(ee, err, type, args) {
  if (typeof ee[kRejection] === 'function') {
    ee[kRejection](err, type, ...args);
  } else {
    // We have to disable the capture rejections mechanism, otherwise
    // we might end up in an infinite loop.
    const prev = ee[kCapture];

    // If the error handler throws, it is not catchable and it
    // will end up in 'uncaughtException'. We restore the previous
    // value of kCapture in case the uncaughtException is present
    // and the exception is handled.
    try {
      ee[kCapture] = false;
      ee.emit('error', err);
    } finally {
      ee[kCapture] = prev;
    }
  }
}

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

function enhanceStackTrace(err, own) {
  let ctorInfo = '';
  try {
    const { name } = this.constructor;
    if (name !== 'EventEmitter')
      ctorInfo = ` on ${name} instance`;
  } catch {
    // Continue regardless of error.
  }
  const sep = `\nEmitted 'error' event${ctorInfo} at:\n`;

  const errStack = ArrayPrototypeSlice(
    StringPrototypeSplit(err.stack, '\n'), 1);
  const ownStack = ArrayPrototypeSlice(
    StringPrototypeSplit(own.stack, '\n'), 1);

  const { len, offset } = identicalSequenceRange(ownStack, errStack);
  if (len > 0) {
    ArrayPrototypeSplice(ownStack, offset + 1, len - 2,
                         '    [... lines matching original stack trace ...]');
  }

  return err.stack + sep + ArrayPrototypeJoin(ownStack, '\n');
}

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

  // TODO - don't handle newListener and removeListener events in fast path for complexity reasons
  if(this[kIsFastPath] && (isEventUnsupportedForFastPath(type) || this[kImpl].isListenerAlreadyExists(type))) {
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

      if(this[kIsFastPath] && (type === 'newListener' || type === 'removeListener' || this[kImpl].isListenerAlreadyExists(type))) {
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

function arrayClone(arr) {
  // At least since V8 8.3, this implementation is faster than the previous
  // which always used a simple for-loop
  switch (arr.length) {
    case 2: return [arr[0], arr[1]];
    case 3: return [arr[0], arr[1], arr[2]];
    case 4: return [arr[0], arr[1], arr[2], arr[3]];
    case 5: return [arr[0], arr[1], arr[2], arr[3], arr[4]];
    case 6: return [arr[0], arr[1], arr[2], arr[3], arr[4], arr[5]];
  }
  return ArrayPrototypeSlice(arr);
}

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

// ---------- FAST

// TODO - should have the same API as slow_event_emitter

// TODO - not supporting
//  1. kCapture - false by default
//  2. _maxListeners (but still need to save the data), should be saved on the parent
//  3. kErrorMonitor - undefined by default
// TODO - add comment for what this is optimized for
class FastEventEmitter {
  // TODO - have a way to support passing _events
  // TODO - add comment on how events are stored
  _events = undefined;
  _eventsCount
  // TODO - convert to symbol and rename
  eventEmitterTranslationLayer = undefined;
  stale = false;

  // TODO - use opts
  constructor(eventEmitterTranslationLayer, opts) {
    this.eventEmitterTranslationLayer = eventEmitterTranslationLayer;
    // TODO - check this:
    // If you're updating this function definition, please also update any
    // re-definitions, such as the one in the Domain module (lib/domain.js).
    if (this._events === undefined ||
      // TODO - this is not correct
      // TODO - change the this here?
      this._events === ObjectGetPrototypeOf(this)._events) {
      // TODO - removed the __proto__ assignment
      this._events = {  };
      this._eventsCount = 0;
      this[kShapeMode] = false;
    } else {
      this[kShapeMode] = true;
    }
  }

  /**
   * Synchronously calls each of the listeners registered
   * for the event.
   * @param {string | symbol} type
   * @param {...any} [args]
   * @returns {boolean}
   */
  emit(type, ...args) {
    const events = this._events;
    if(type === 'error' && events?.error === undefined) {
      throwErrorOnMissingErrorHandler.apply(this, args);
      // TODO - add assertion that should not reach here;
      return false;
    }

    // TODO(rluvaton): will it be faster to add check if events is undefined instead?
    const handler = events?.[type];

    if(handler === undefined) {
      return false;
    }

    // TODO - change
    handler.apply(this.eventEmitterTranslationLayer, args);

    return true;
  };

  // TODO - should switch to slow mode
  isListenerAlreadyExists(type) {
    return this._events?.[type] !== undefined;
  }

  /**
   * Adds a listener to the event emitter.
   * @param {string | symbol} type
   * @param {Function} listener
   * @param {boolean} prepend not used here as we are in fast mode and only have single listener
   * @returns {EventEmitter}
   */
  addListener(type, listener, prepend = undefined) {
    // TODO - add validation before getting here
    // TODO - if got here that can add without switching to slow mode
    let events;

    events = this._events;
    // TODO - simplify this
    if (events === undefined) {
      events = this._events = {
        // TODO - change this?
        __proto__: null
      };
      this._eventsCount = 0;
    } else {
      // TODO(rluvaton): support addListener and removeListener events
      // // To avoid recursion in the case that type === "newListener"! Before
      // // adding it to the listeners, first emit "newListener".
      // if (events.newListener !== undefined) {
      //   // TODO(rluvaton): use apply to pass the parent eventEmitter
      //   this.emit('newListener', type,
      //     listener.listener ?? listener);
      //
      //
      //   // TODO - here we could already have aq listener to the same value so need to switch to slow mode
      //
      //   if(this.stale) {
      //     // TODO - continue in the slow mode
      //     this.eventEmitterTranslationLayer[kContinueInSlowMode](type, listener, prepend);
      //     return;
      //   }
      //
      //   if(this._events[type]) {
      //     // TODO(rluvaton): try to find a better way
      //     this.stale = true;
      //     this.eventEmitterTranslationLayer[kContinueInSlowMode](type, listener, prepend);
      //
      //     // TODO(rluvaton): continue in the slow mode
      //     return;
      //   }
      //
      //   // Re-assign `events` because a newListener handler could have caused the
      //   // this._events to be assigned to a new object
      //   // TODO(rluvaton): change this
      //   events = this._events;
      // }
    }

    // Optimize the case of one listener. Don't need the extra array object.
    events[type] = listener;
    ++this._eventsCount;

  }

  /**
   * Removes the specified `listener`.
   * @param {string | symbol} type
   * @param {Function} listener
   * @returns {EventEmitter}
   */
  removeListener(type, listener) {
    // TODO - add validation before getting here
    // TODO - parent function should return `this`

    const events = this._events;
    if (events === undefined)
      return undefined;

    const list = events[type];
    if (list === undefined || (list !== listener && list.listener !== listener))
      return undefined;

    this._eventsCount -= 1;

    if (this[kShapeMode]) {
      events[type] = undefined;
    } else if (this._eventsCount === 0) {
      // TODO - keep this?
      this._events = {__proto__: null};
    } else {
      delete events[type];
      // TODO(rluvaton): support addListener and removeListener events
      // if (events.removeListener)
      //   // TODO(rluvaton): use apply to pass the parent eventEmitter
      //   this.emit('removeListener', type, list.listener || listener);
    }
  };

  /**
   * Removes all listeners from the event emitter. (Only
   * removes listeners for a specific event name if specified
   * as `type`).
   * @param {string | symbol} [type]
   * @returns {EventEmitter}
   */
  removeAllListeners(type) {
    // TODO - parent function should return `this`
    const events = this._events;
    if (events === undefined)
      return undefined;

    // Not listening for removeListener, no need to emit
    if (events.removeListener === undefined) {
      if (arguments.length === 0) {
        this._events = { __proto__: null };
        this._eventsCount = 0;
      } else if (events[type] !== undefined) {
        if (--this._eventsCount === 0)
          this._events = { __proto__: null };
        else
          delete events[type];
      }
      this[kShapeMode] = false;

      return undefined;
    }

    // Emit removeListener for all listeners on all events
    if (arguments.length === 0) {
      for (const key of ReflectOwnKeys(events)) {
        if (key === 'removeListener') continue;
        this.removeAllListeners(key);
      }
      this.removeAllListeners('removeListener');
      this._events = { __proto__: null };
      this._eventsCount = 0;
      this[kShapeMode] = false;

      return undefined;
    }

    const listeners = events[type];

    if (typeof listeners === 'function') {
      this.removeListener(type, listeners);
    }
  };
}

// --------------- SLOW


// TODO - should have the same API as slow_event_emitter

// TODO - not supporting
//  2. _maxListeners (but still need to save the data), should be saved on the parent
//  3. kErrorMonitor - undefined by default
// TODO - add comment for what this is optimized for
class SlowEventEmitter {
  // TODO - have a way to support passing _events
  // TODO - add comment on how events are stored
  _events = undefined;
  _eventsCount = 0
  // TODO - convert to symbol and rename
  eventEmitterTranslationLayer = undefined;

  /**
   *
   * @param {FastEventEmitter} fastEventEmitter
   */
  static fromFastEventEmitter(fastEventEmitter) {
    const eventEmitter = new SlowEventEmitter(fastEventEmitter.eventEmitterTranslationLayer);

    // TODO - add missing
    eventEmitter._events = fastEventEmitter._events;
    eventEmitter._eventsCount = fastEventEmitter._eventsCount;
    eventEmitter[kShapeMode] = fastEventEmitter[kShapeMode];

    return eventEmitter;
  }

  // TODO - use opts
  constructor(eventEmitterTranslationLayer, opts) {
    this.eventEmitterTranslationLayer = eventEmitterTranslationLayer;
    // TODO -
  }

  /**
   * Synchronously calls each of the listeners registered
   * for the event.
   * @param {string | symbol} type
   * @param {...any} [args]
   * @returns {boolean}
   */
  emit(type, ...args) {
    let doError = (type === 'error');

    const events = this._events;
    if (events !== undefined) {
      if (doError && events[kErrorMonitor] !== undefined)
        this.emit(kErrorMonitor, ...args);
      doError = (doError && events.error === undefined);
    } else if (!doError)
      return false;

    // If there is no 'error' event listener then throw.
    if (doError) {
      throwErrorOnMissingErrorHandler(args);
    }

    const handler = events[type];

    if (handler === undefined)
      return false;

    if (typeof handler === 'function') {
      const result = handler.apply(this, args);

      // We check if result is undefined first because that
      // is the most common case so we do not pay any perf
      // penalty
      if (result !== undefined && result !== null) {
        addCatch(this, result, type, args);
      }
    } else {
      const len = handler.length;
      const listeners = arrayClone(handler);
      for (let i = 0; i < len; ++i) {
        const result = listeners[i].apply(this, args);

        // We check if result is undefined first because that
        // is the most common case so we do not pay any perf
        // penalty.
        // This code is duplicated because extracting it away
        // would make it non-inlineable.
        if (result !== undefined && result !== null) {
          addCatch(this, result, type, args);
        }
      }
    }

    return true;
  };

  /**
   * Adds a listener to the event emitter.
   * @param {string | symbol} type
   * @param {Function} listener
   * @returns {EventEmitter}
   */
  addListener(type, listener, prepend) {
    debugger;
    const target = this.eventEmitterTranslationLayer;
    let m;
    let events;
    let existing;

    events = this._events;
    if (events === undefined) {
      events = this._events = {__proto__: null};
      this._eventsCount = 0;
    } else {
      // To avoid recursion in the case that type === "newListener"! Before
      // adding it to the listeners, first emit "newListener".
      if (events.newListener !== undefined) {
        // TODO - emit this to be the eventEmitterTranslationLayer
        this.eventEmitterTranslationLayer.emit('newListener', type,
          listener.listener ?? listener);

        // Re-assign `events` because a newListener handler could have caused the
        // this._events to be assigned to a new object
        events = this._events;
      }
      existing = events[type];
    }

    if (existing === undefined) {
      // Optimize the case of one listener. Don't need the extra array object.
      events[type] = listener;
      ++this._eventsCount;
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
      // TODO - move away from this
      m = _getMaxListeners(target);
      if (m > 0 && existing.length > m && !existing.warned) {
        existing.warned = true;
        // No error code for this since it is a Warning
        const w = genericNodeError(
          `Possible EventEmitter memory leak detected. ${existing.length} ${String(type)} listeners ` +
          `added to ${inspect(target, {depth: -1})}. Use emitter.setMaxListeners() to increase limit`,
          {name: 'MaxListenersExceededWarning', emitter: target, type: type, count: existing.length});
        process.emitWarning(w);
      }
    }

    return this;
  }

  /**
   * Removes the specified `listener`.
   * @param {string | symbol} type
   * @param {Function} listener
   * @returns {EventEmitter}
   */
  removeListener(type, listener) {
    const events = this._events;
    if (events === undefined)
      return undefined;

    const list = events[type];
    if (list === undefined)
      return undefined;

    if (list === listener || list.listener === listener) {
      this._eventsCount -= 1;

      if (this[kShapeMode]) {
        events[type] = undefined;
      } else if (this._eventsCount === 0) {
        this._events = {__proto__: null};
      } else {
        delete events[type];
        if (events.removeListener)
          this.emit('removeListener', type, list.listener || listener);
      }
    } else if (typeof list !== 'function') {
      let position = -1;

      for (let i = list.length - 1; i >= 0; i--) {
        if (list[i] === listener || list[i].listener === listener) {
          position = i;
          break;
        }
      }

      if (position < 0)
        return this;

      if (position === 0)
        list.shift();
      else {
        if (spliceOne === undefined)
          spliceOne = require('internal/util').spliceOne;
        spliceOne(list, position);
      }

      if (list.length === 1)
        events[type] = list[0];

      if (events.removeListener !== undefined)
        this.emit('removeListener', type, listener);
    }

    return undefined;
  };

  /**
   * Removes all listeners from the event emitter. (Only
   * removes listeners for a specific event name if specified
   * as `type`).
   * @param {string | symbol} [type]
   * @returns {EventEmitter}
   */
  removeAllListeners(type) {
    const events = this._events;
    if (events === undefined)
      return undefined;

    // Not listening for removeListener, no need to emit
    if (events.removeListener === undefined) {
      if (arguments.length === 0) {
        this._events = {__proto__: null};
        this._eventsCount = 0;
      } else if (events[type] !== undefined) {
        if (--this._eventsCount === 0)
          this._events = {__proto__: null};
        else
          delete events[type];
      }
      this[kShapeMode] = false;
      return undefined;
    }

    // Emit removeListener for all listeners on all events
    if (arguments.length === 0) {
      for (const key of ReflectOwnKeys(events)) {
        if (key === 'removeListener') continue;
        this.removeAllListeners(key);
      }
      this.removeAllListeners('removeListener');
      this._events = {__proto__: null};
      this._eventsCount = 0;
      this[kShapeMode] = false;
      return undefined;
    }

    const listeners = events[type];

    if (typeof listeners === 'function') {
      this.removeListener(type, listeners);
    } else if (listeners !== undefined) {
      // LIFO order
      for (let i = listeners.length - 1; i >= 0; i--) {
        this.removeListener(type, listeners[i]);
      }
    }

    return undefined;
  };
}



function addCatch(that, promise, type, args) {
  if (!that.eventEmitterTranslationLayer[kCapture]) {
    return;
  }

  // Handle Promises/A+ spec, then could be a getter
  // that throws on second use.
  try {
    const then = promise.then;

    if (typeof then === 'function') {
      then.call(promise, undefined, function (err) {
        // The callback is called with nextTick to avoid a follow-up
        // rejection from this promise.
        process.nextTick(emitUnhandledRejectionOrErr, that, err, type, args);
      });
    }
  } catch (err) {
    that.emit('error', err);
  }
}

function emitUnhandledRejectionOrErr(ee, err, type, args) {
  // TODO - kRejection should move to parent
  if (typeof ee.eventEmitterTranslationLayer[kRejection] === 'function') {
    ee.eventEmitterTranslationLayer[kRejection](err, type, ...args);
  } else {
    // We have to disable the capture rejections mechanism, otherwise
    // we might end up in an infinite loop.
    const prev = ee.eventEmitterTranslationLayer[kCapture];

    // If the error handler throws, it is not catchable and it
    // will end up in 'uncaughtException'. We restore the previous
    // value of kCapture in case the uncaughtException is present
    // and the exception is handled.
    try {
      ee.eventEmitterTranslationLayer[kCapture] = false;
      ee.emit('error', err);
    } finally {
      ee.eventEmitterTranslationLayer[kCapture] = prev;
    }
  }
}

// ------- Internal helpers


// TODO - move this to a different file
// TODO - rename
function throwErrorOnMissingErrorHandler(args) {
  let er;
  if (args.length > 0)
    er = args[0];

  if (er instanceof Error) {
    try {
      const capture = {};
      ErrorCaptureStackTrace(capture, EventEmitter.prototype.emit);
      ObjectDefineProperty(er, kEnhanceStackBeforeInspector, {
        __proto__: null,
        value: FunctionPrototypeBind(enhanceStackTrace, this, er, capture),
        configurable: true,
      });
    } catch {
      // Continue regardless of error.
    }

    // Note: The comments on the `throw` lines are intentional, they show
    // up in Node's output if this results in an unhandled exception.
    throw er; // Unhandled 'error' event
  }

  let stringifiedEr;
  try {
    stringifiedEr = inspect(er);
  } catch {
    stringifiedEr = er;
  }

  // At least give some kind of context to the user
  const err = new ERR_UNHANDLED_ERROR(stringifiedEr);
  err.context = er;
  throw err; // Unhandled 'error' event
}


// TODO - move this
function enhanceStackTrace(err, own) {
  let ctorInfo = '';
  try {
    const { name } = this.constructor;
    if (name !== 'EventEmitter')
      ctorInfo = ` on ${name} instance`;
  } catch {
    // Continue regardless of error.
  }
  const sep = `\nEmitted 'error' event${ctorInfo} at:\n`;

  const errStack = ArrayPrototypeSlice(
    StringPrototypeSplit(err.stack, '\n'), 1);
  const ownStack = ArrayPrototypeSlice(
    StringPrototypeSplit(own.stack, '\n'), 1);

  const { len, offset } = identicalSequenceRange(ownStack, errStack);
  if (len > 0) {
    ArrayPrototypeSplice(ownStack, offset + 1, len - 2,
      '    [... lines matching original stack trace ...]');
  }

  return err.stack + sep + ArrayPrototypeJoin(ownStack, '\n');
}


function arrayClone(arr) {
  // At least since V8 8.3, this implementation is faster than the previous
  // which always used a simple for-loop
  switch (arr.length) {
    case 2:
      return [arr[0], arr[1]];
    case 3:
      return [arr[0], arr[1], arr[2]];
    case 4:
      return [arr[0], arr[1], arr[2], arr[3]];
    case 5:
      return [arr[0], arr[1], arr[2], arr[3], arr[4]];
    case 6:
      return [arr[0], arr[1], arr[2], arr[3], arr[4], arr[5]];
  }
  return ArrayPrototypeSlice(arr);
}

// --------- Current



// TODO - change this back to function prototype
// class EventEmitter {
//   // [kImpl] = undefined;
//   // [kIsFastPath] = true;
//   // [kCaptureValue] = false;
//
//   // _maxListeners = 0;
//   //
//   // // TODO - backwards compat
//   // get _eventsCount() {
//   //   return this[kImpl]._eventsCount;
//   // }
//   //
//   // set _eventsCount(n) {
//   //   // TODO - deprecate this
//   //   this[kImpl]._eventsCount = n;
//   // }
//   //
//   // get _events() {
//   //   return this[kImpl]._events;
//   // }
//   //
//   // set _events(events) {
//   //   // TODO - might need to change to slow path
//   //   // TODO - deprecate this
//   //   this[kImpl]._events = events;
//   // }
//
//   // TODO - add backwards compat
//   //
//   // constructor(opt) {
//   //   this[kImpl] = new FastEventEmitter(this, opt);
//   //   this[kIsFastPath] = true;
//   //   // TODO - call init
//   // }
//
//   /**
//    * Increases the max listeners of the event emitter.
//    * @param {number} n
//    * @returns {EventEmitter}
//    */
//   setMaxListeners(n) {
//     validateNumber(n, 'setMaxListeners', 0);
//     this._maxListeners = n;
//     return this;
//   }
//
//   /**
//    * Returns the current max listener value for the event emitter.
//    * @returns {number}
//    */
//   getMaxListeners() {
//     return _getMaxListeners(this);
//   }
//
//   /**
//    * Synchronously calls each of the listeners registered
//    * for the event.
//    * @param {string | symbol} type
//    * @param {...any} [args]
//    * @returns {boolean}
//    */
//   emit(type, ...args) {
//     return this[kImpl].emit(type, ...args);
//   }
//
//   /**
//    * Adds a listener to the event emitter.
//    * @param {string | symbol} type
//    * @param {Function} listener
//    * @returns {EventEmitter}
//    */
//   addListener(type, listener) {
//     checkListener(listener);
//
//     if(this[kIsFastPath] && this[kImpl].isListenerAlreadyExists(type)) {
//       this[kSwitchToSlowPath]();
//     }
//
//     return this[kImpl].addListener(type, listener);
//   }
//
//   /**
//    * Adds a listener to the event emitter.
//    * @param {string | symbol} type
//    * @param {Function} listener
//    * @returns {EventEmitter}
//    */
//   // TODO - change to on = addListener
//   on(type, listener) {
//     checkListener(listener);
//
//     if(this[kIsFastPath] && this[kImpl].isListenerAlreadyExists(type)) {
//       this[kSwitchToSlowPath]();
//     }
//
//     return this[kImpl].addListener(type, listener);
//   }
//
//
//   /**
//    * Adds the `listener` function to the beginning of
//    * the listeners array.
//    * @param {string | symbol} type
//    * @param {Function} listener
//    * @returns {EventEmitter}
//    */
//   prependListener(type, listener) {
//     checkListener(listener);
//
//     if(this[kIsFastPath] && this[kImpl].isListenerAlreadyExists(type)) {
//       this[kSwitchToSlowPath]();
//     }
//
//     return this[kImpl].addListener(type, listener, true);
//   }
//
//
//   /**
//    * Adds a one-time `listener` function to the event emitter.
//    * @param {string | symbol} type
//    * @param {Function} listener
//    * @returns {EventEmitter}
//    */
//   once(type, listener) {
//     checkListener(listener);
//
//     this.on(type, _onceWrap(this, type, listener));
//     return this;
//   };
//
//
//   /**
//    * Adds a one-time `listener` function to the beginning of
//    * the listeners array.
//    * @param {string | symbol} type
//    * @param {Function} listener
//    * @returns {EventEmitter}
//    */
//   prependOnceListener(type, listener) {
//     checkListener(listener);
//
//     this.prependListener(type, _onceWrap(this, type, listener));
//     return this;
//   }
//
//
//   /**
//    * Removes the specified `listener` from the listeners array.
//    * @param {string | symbol} type
//    * @param {Function} listener
//    * @returns {EventEmitter}
//    */
//   removeListener(type, listener) {
//     checkListener(listener);
//
//     this[kImpl].removeListener(type, listener);
//
//     return this;
//   }
//
//   // TODO - EventEmitter.prototype.off = EventEmitter.prototype.removeListener;
//   off(type, listener) {
//     // TODO - did remove listener had checkListener
//     checkListener(listener);
//
//     this[kImpl].removeListener(type, listener);
//
//     return this;
//   }
//
//   /**
//    * Removes all listeners from the event emitter. (Only
//    * removes listeners for a specific event name if specified
//    * as `type`).
//    * @param {string | symbol} [type]
//    * @returns {EventEmitter}
//    */
//   removeAllListeners(type) {
//     this[kImpl].removeAllListeners(type);
//     return this;
//   }
//
//   /**
//    * Returns a copy of the array of listeners for the event name
//    * specified as `type`.
//    * @param {string | symbol} type
//    * @returns {Function[]}
//    */
//   listeners(type) {
//     return _listeners(this, type, true);
//   };
//
//   /**
//    * Returns a copy of the array of listeners and wrappers for
//    * the event name specified as `type`.
//    * @param {string | symbol} type
//    * @returns {Function[]}
//    */
//   rawListeners(type) {
//     return _listeners(this, type, false);
//   };
//
//
//   /**
//    * Returns the number of listeners listening to event name
//    * specified as `type`.
//    * @param {string | symbol} type
//    * @param {Function} listener
//    * @returns {number}
//    */
//   listenerCount(type, listener) {
//     // TODO - change this back to prototype
//     return listenerCount.call(this, type, listener);
//   }
//
//   /**
//    * Returns an array listing the events for which
//    * the emitter has registered listeners.
//    * @returns {any[]}
//    */
//   eventNames() {
//     return this._eventsCount > 0 ? ReflectOwnKeys(this._events) : [];
//   };
//
//   [kSwitchToSlowPath]() {
//     if(!this[kIsFastPath]) {
//       return;
//     }
//
//     this[kIsFastPath] = false;
//     this[kImpl] = SlowEventEmitter.fromFastEventEmitter(this[kImpl]);
//   }
// }
//
// // The default for captureRejections is false
// ObjectDefineProperty(EventEmitter.prototype, kCapture, {
//   __proto__: null,
//   get() {
//     return this[kCaptureValue];
//   },
//   set(value) {
//     this[kCaptureValue] = value;
//
//     if (value) {
//       this[kSwitchToSlowPath]();
//     }
//   },
//   writable: true,
//   enumerable: false,
// });
//

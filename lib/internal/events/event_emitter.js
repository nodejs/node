'use strict';

const {
  ArrayPrototypeUnshift,
  Boolean,
  FunctionPrototypeCall,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ReflectApply,
  ReflectOwnKeys,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  validateBoolean,
  validateFunction,
  validateNumber,
  validateString,
} = require('internal/validators');
const {
  kCapture,
  kErrorMonitor,
  kShapeMode,
  kMaxEventTargetListeners,
  kMaxEventTargetListenersWarned,
  kImpl,
  kIsFastPath,
  kSwitchToSlowPath,
  kInitialEvents,
  kRejection,
} = require('internal/events/symbols');
const {
  arrayClone,
  throwErrorOnMissingErrorHandler,
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

module.exports = {
  EventEmitter,
  _getMaxListeners,
};
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
  [kInitialEvents]: {
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
      return this[kImpl]?._events;
    },
    set: function(arg) {
      // If using the _events setter move to slow path to avoid bugs with incorrect shape or functions
      // Users should not interact with _events directly
      EventEmitter.prototype[kSwitchToSlowPath].call(this);
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
      if (this[kImpl] === undefined) {
        return 0;
      }
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
  let thisPrototype;

  if (this[kImpl] === undefined || (thisPrototype = ObjectGetPrototypeOf(this))[kImpl] === this[kImpl]) {
    this[kImpl] = new FastEventEmitter(this);
    this[kIsFastPath] = true;
  }

  const thisEvents = this._events;
  const impl = this[kImpl];
  const missingEvents = thisEvents === undefined;

  if (missingEvents && this[kInitialEvents] !== undefined) {
    impl._events = this[kInitialEvents];
    this[kInitialEvents] = undefined;
    impl[kShapeMode] = true;
  } else if (missingEvents ||
      thisEvents === (thisPrototype || ObjectGetPrototypeOf(this))._events) {
    // In fast path we don't want to have __proto__: null as it will cause the object to be in dictionary mode
    // and will slow down the access to the properties by a lot (around 2x)
    impl._events = this[kIsFastPath] ? {} : { __proto__: null };
    impl._eventsCount = 0;
    impl[kShapeMode] = false;
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
    this[kImpl] = SlowEventEmitter.fromFastEventEmitter(this[kImpl]);
    return;
  }

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
  // Users can call emit in the constructor before even calling super causing this[kImpl] to be undefined
  const impl = this[kImpl];

  // The order here is important as impl === undefined is slower than type === 'error'
  if (type === 'error' && impl === undefined) {
    throwErrorOnMissingErrorHandler.apply(this, args);
  }

  return impl !== undefined ? impl.emit.apply(impl, arguments) : false;
};

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
  if (this[kImpl] === undefined) {
    EventEmitter.init.apply(this);
  }

  // If the listener is already added,
  // switch to slow path as the fast path optimized for single listener for each event
  if (this[kIsFastPath] && (isEventUnsupportedForFastPath(type) || this[kImpl].isListenerAlreadyExists(type))) {
    EventEmitter.prototype[kSwitchToSlowPath].call(this);
  }

  this[kImpl].addListener(type, listener);

  return this;
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
      if (this[kImpl] === undefined) {
        EventEmitter.init.apply(this);
      }

      // If the listener is already added,
      // switch to slow path as the fast path optimized for single listener for each event
      if (this[kIsFastPath] && (isEventUnsupportedForFastPath(type) || this[kImpl].isListenerAlreadyExists(type))) {
        EventEmitter.prototype[kSwitchToSlowPath].call(this);
      }

      this[kImpl].addListener(type, listener, true);

      return this;
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
      this[kImpl].removeAllListeners(arguments);
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

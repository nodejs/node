'use strict';

const {
  ObjectAssign,
  ObjectPrototype,
  ObjectPrototypeHasOwnProperty,
  ReflectOwnKeys,
  String,
} = primordials;

let spliceOne;

const { kErrorMonitor, kShapeMode } = require('internal/events/symbols');
const {
  arrayClone,
  addCatch,
  throwErrorOnMissingErrorHandler,
} = require('internal/events/shared_internal_event_emitter');
const { genericNodeError } = require('internal/errors');
const { inspect } = require('internal/util/inspect');

let _getMaxListeners;
let protoExists = true;

try {
  // Can't use getOptionValue('--disable-proto') here as it's not available yet

  // Checking whether --disable-proto is equal 'throw'
  // eslint-disable-next-line no-proto
  protoExists = !!({}).__proto__;

  // Checking whether --disable-proto is equal 'delete'
  protoExists = ObjectPrototypeHasOwnProperty(ObjectPrototype, '__proto__');
} catch {
  protoExists = false;
}

function SlowEventEmitter(eventEmitterTranslationLayer, events = undefined) {
  this.eventEmitterTranslationLayer = eventEmitterTranslationLayer;
  this._events = events;
}

SlowEventEmitter.prototype._events = undefined;
SlowEventEmitter.prototype._eventsCount = 0;
SlowEventEmitter.prototype.eventEmitterTranslationLayer = undefined;
SlowEventEmitter.prototype[kShapeMode] = undefined;

/**
 * @param {FastEventEmitter} fastEventEmitter
 */
SlowEventEmitter.fromFastEventEmitter = function fromFastEventEmitter(fastEventEmitter) {
  let events = fastEventEmitter._events;
  // To support weird cases where the event name is object keys such as __defineGetter__
  if (protoExists) {
  // eslint-disable-next-line no-proto
    events.__proto__ = null;
  } else {
    events = ObjectAssign({ __proto__: null }, events);
  }
  const eventEmitter = new SlowEventEmitter(fastEventEmitter.eventEmitterTranslationLayer, events);

  eventEmitter._eventsCount = fastEventEmitter._eventsCount;
  eventEmitter[kShapeMode] = fastEventEmitter[kShapeMode];

  return eventEmitter;
};

/**
 * Synchronously calls each of the listeners registered
 * for the event.
 * @param {string | symbol} type
 * @param {...any} [args]
 * @returns {boolean}
 */
SlowEventEmitter.prototype.emit = function emit(type, ...args) {
  let doError = (type === 'error');
  const eventEmitterTranslationLayer = this.eventEmitterTranslationLayer;

  const events = this._events;
  if (events !== undefined) {
    if (doError && events[kErrorMonitor] !== undefined)
      this.emit(kErrorMonitor, ...args);
    doError = (doError && events.error === undefined);
  } else if (!doError)
    return false;

  // If there is no 'error' event listener then throw.
  if (doError) {
    throwErrorOnMissingErrorHandler.apply(eventEmitterTranslationLayer, args);
  }

  const handler = events[type];

  if (handler === undefined)
    return false;

  if (typeof handler === 'function') {
    const result = handler.apply(eventEmitterTranslationLayer, args);

    // We check if result is undefined first because that
    // is the most common case so we do not pay any perf
    // penalty
    if (result !== undefined && result !== null) {
      addCatch(eventEmitterTranslationLayer, result, type, args);
    }
  } else {
    const len = handler.length;
    const listeners = arrayClone(handler);
    for (let i = 0; i < len; ++i) {
      const result = listeners[i].apply(eventEmitterTranslationLayer, args);

      // We check if result is undefined first because that
      // is the most common case so we do not pay any perf
      // penalty.
      // This code is duplicated because extracting it away
      // would make it non-inlineable.
      if (result !== undefined && result !== null) {
        addCatch(eventEmitterTranslationLayer, result, type, args);
      }
    }
  }

  return true;
};

/**
 * Adds a listener to the event emitter.
 * @param {string | symbol} type
 * @param {Function} listener
 * @param {boolean} prepend
 */
SlowEventEmitter.prototype.addListener = function addListener(type, listener, prepend) {
  const target = this.eventEmitterTranslationLayer;
  let m;
  let events;
  let existing;

  events = this._events;
  if (events === undefined) {
    events = this._events = { __proto__: null };
    this._eventsCount = 0;
  } else {
    // To avoid recursion in the case that type === "newListener"! Before
    // adding it to the listeners, first emit "newListener".
    if (events.newListener !== undefined) {
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
    _getMaxListeners ??= require('internal/events/event_emitter')._getMaxListeners;
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
};

/**
 * Removes the specified `listener`.
 * @param {string | symbol} type
 * @param {Function} listener
 * @returns {EventEmitter}
 */
SlowEventEmitter.prototype.removeListener = function removeListener(type, listener) {
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
      this._events = { __proto__: null };
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
      return undefined;

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
SlowEventEmitter.prototype.removeAllListeners = function removeAllListeners(type) {
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
      this.eventEmitterTranslationLayer.removeAllListeners(key);
    }
    this.eventEmitterTranslationLayer.removeAllListeners('removeListener');
    this._events = { __proto__: null };
    this._eventsCount = 0;
    this[kShapeMode] = false;
    return undefined;
  }

  const listeners = events[type];

  if (typeof listeners === 'function') {
    this.eventEmitterTranslationLayer.removeListener(type, listeners);
  } else if (listeners !== undefined) {
    // LIFO order
    for (let i = listeners.length - 1; i >= 0; i--) {
      this.eventEmitterTranslationLayer.removeListener(type, listeners[i]);
    }
  }

  return undefined;
};


module.exports = {
  SlowEventEmitter,
};

'use strict';

const { kShapeMode, kErrorMonitor } = require('internal/events/symbols');
const {
  throwErrorOnMissingErrorHandler,
  addCatch,
} = require('internal/events/shared_internal_event_emitter');

/**
 * This class is optimized for the case where there is only a single listener for each event.
 * it supports kCapture
 * but does not support event types that {@link isEventUnsupportedForFastPath} returns false for
 */
function FastEventEmitter(eventEmitterTranslationLayer, _events) {
  this.eventEmitterTranslationLayer = eventEmitterTranslationLayer;
  this._events = _events;
}

/**
 * The events are stored here as Record<string, function | undefined>
 */
FastEventEmitter.prototype._events = undefined;
FastEventEmitter.prototype[kShapeMode] = undefined;
FastEventEmitter.prototype._eventsCount = 0;
FastEventEmitter.prototype.eventEmitterTranslationLayer = undefined;

/**
 * Synchronously calls each of the listeners registered
 * for the event.
 * @param {string | symbol} type
 * @param {...any} [args]
 * @returns {boolean}
 */
FastEventEmitter.prototype.emit = function emit(type, ...args) {
  const events = this._events;

  if (type === 'error' && events?.error === undefined) {
    throwErrorOnMissingErrorHandler.apply(this.eventEmitterTranslationLayer, args);
  }

  const handler = events[type];

  if (handler === undefined) {
    return false;
  }

  const result = handler.apply(this.eventEmitterTranslationLayer, args);

  // We check if result is undefined first because that
  // is the most common case so we do not pay any perf
  // penalty
  if (result !== undefined && result !== null) {
    addCatch(this.eventEmitterTranslationLayer, result, type, args);
  }

  return true;
};

FastEventEmitter.prototype.isListenerAlreadyExists = function isListenerAlreadyExists(type) {
  return this._events?.[type] !== undefined;
};

/**
 * Adds a listener to the event emitter.
 * @param {string | symbol} type
 * @param {Function} listener
 * @param {boolean} prepend not used here as we are in fast mode and only have single listener
 */
FastEventEmitter.prototype.addListener = function addListener(type, listener, prepend = undefined) {
  let events;

  events = this._events;
  if (events === undefined) {
    events = this._events = {};
    this._eventsCount = 0;

    // Not emitting `newListener` here as in fast path we don't have it
  }

  // Optimize the case of one listener. Don't need the extra array object.
  events[type] = listener;
  ++this._eventsCount;
};

/**
 * Removes the specified `listener`.
 * @param {string | symbol} type
 * @param {Function} listener
 */
FastEventEmitter.prototype.removeListener = function removeListener(type, listener) {
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
    this._events = { };
  } else {
    delete events[type];
    // Not emitting `removeListener` here as in fast path we don't have it
  }
};


/**
 * Removes all listeners from the event emitter. (Only
 * removes listeners for a specific event name if specified
 * as `type`).
 * @param {string | symbol} [type]
 */
FastEventEmitter.prototype.removeAllListeners = function removeAllListeners(type) {
  const events = this._events;
  if (events === undefined)
    return undefined;

  if (arguments.length === 0) {
    this._events = { };
    this._eventsCount = 0;
  } else if (events[type] !== undefined) {
    if (--this._eventsCount === 0)
      this._events = { };
    else
      delete events[type];
  }

  this[kShapeMode] = false;

  return undefined;
};


function isEventUnsupportedForFastPath(type) {
  return (
    // Not supporting newListener and removeListener events in fast path
    // as they can add new listeners in the middle of the process
    type === 'newListener' ||
    type === 'removeListener' ||

    // Not supporting errorMonitor event as it has special handling
    type === kErrorMonitor ||

    // Not supporting Object prototype keys as fast path object is not an object with null prototype
    type === 'constructor' ||
    type === '__defineGetter__' ||
    type === '__defineSetter__' ||
    type === 'hasOwnProperty' ||
    type === '__lookupGetter__' ||
    type === '__lookupSetter__' ||
    type === 'isPrototypeOf' ||
    type === 'propertyIsEnumerable' ||
    type === 'toString' ||
    type === 'valueOf' ||
    type === '__proto__' ||
    type === 'toLocaleString'
  );
}


module.exports = {
  FastEventEmitter,
  isEventUnsupportedForFastPath,
};

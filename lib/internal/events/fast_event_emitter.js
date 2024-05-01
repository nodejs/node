'use strict';

const { kShapeMode } = require('internal/events/symbols');
const {
  throwErrorOnMissingErrorHandler,
  addCatch,
} = require('internal/events/shared_internal_event_emitter');

// TODO - should have the same API as slow_event_emitter
/**
 * This class is optimized for the case where there is only a single listener for each event.
 * it support kCapture
 * but does not support event types with the following names:
 * 1. 'newListener' and 'removeListener' as they can cause another event to be added
 *     and making the code less predictable thus harder to optimize
 * 2. kErrorMonitor as it has special handling
 */
class FastEventEmitter {
  /**
   * The events are stored here as Record<string, function | Once object | undefined>
   */
  _events = undefined;
  _eventsCount;
  // TODO - convert to symbol and rename
  eventEmitterTranslationLayer = undefined;
  [kShapeMode] = undefined;

  constructor(eventEmitterTranslationLayer, _events = undefined) {
    this.eventEmitterTranslationLayer = eventEmitterTranslationLayer;
    this._events = _events;
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
    if (type === 'error' && events?.error === undefined) {
      throwErrorOnMissingErrorHandler.apply(this.eventEmitterTranslationLayer, args);
      // TODO - add assertion that should not reach here;
      return false;
    }

    // TODO(rluvaton): will it be faster to add check if events is undefined instead?
    const handler = events?.[type];

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
  }

  isListenerAlreadyExists(type) {
    return this._events?.[type] !== undefined;
  }

  /**
   * Adds a listener to the event emitter.
   * @param {string | symbol} type
   * @param {Function} listener
   * @param {boolean} prepend not used here as we are in fast mode and only have single listener
   */
  addListener(type, listener, prepend = undefined) {
    let events;

    events = this._events;
    // TODO - simplify this
    if (events === undefined) {
      events = this._events = {};
      this._eventsCount = 0;

      // Not emitting `newListener` here as in fast path we don't have it
    }

    // Optimize the case of one listener. Don't need the extra array object.
    events[type] = listener;
    ++this._eventsCount;
  }

  /**
   * Removes the specified `listener`.
   * @param {string | symbol} type
   * @param {Function} listener
   */
  removeListener(type, listener) {

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
  }

  /**
   * Removes all listeners from the event emitter. (Only
   * removes listeners for a specific event name if specified
   * as `type`).
   * @param {string | symbol} [type]
   */
  removeAllListeners(type) {
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
  }
}

module.exports = {
  FastEventEmitter,
};

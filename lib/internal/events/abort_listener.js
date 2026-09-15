'use strict';

const {
  ObjectFreeze,
  SymbolDispose,
} = primordials;
const {
  validateAbortSignal,
  validateFunction,
} = require('internal/validators');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

let queueMicrotask;
let kResistStopPropagation;
let abortListenerOptions;
let eventTarget;

/**
 * @param {AbortSignal} signal
 * @param {EventListener} listener
 * @returns {Disposable}
 */
function addAbortListener(signal, listener) {
  if (signal === undefined) {
    throw new ERR_INVALID_ARG_TYPE('signal', 'AbortSignal', signal);
  }
  validateAbortSignal(signal, 'signal');
  validateFunction(listener, 'listener');

  let removeEventListener;
  if (signal.aborted) {
    queueMicrotask ??= require('internal/process/task_queues').queueMicrotask;
    eventTarget ??= require('internal/event_target');
    let disposed = false;
    queueMicrotask(() => {
      if (disposed) return;
      const { Event, kIsBeingDispatched, kTarget, kTrustEvent } = eventTarget;
      const event = new Event('abort', { [kTrustEvent]: true });
      event[kTarget] = signal;
      event[kIsBeingDispatched] = true;
      listener(event);
      event[kIsBeingDispatched] = false;
    });
    removeEventListener = () => {
      disposed = true;
    };
  } else {
    kResistStopPropagation ??= require('internal/event_target').kResistStopPropagation;
    abortListenerOptions ??= ObjectFreeze({ __proto__: null, once: true, [kResistStopPropagation]: true });
    // TODO(atlowChemi) add { subscription: true } and return directly
    signal.addEventListener('abort', listener, abortListenerOptions);
    removeEventListener = () => {
      signal.removeEventListener('abort', listener);
    };
  }
  return {
    __proto__: null,
    [SymbolDispose]() {
      removeEventListener?.();
    },
  };
}

module.exports = {
  __proto__: null,
  addAbortListener,
};

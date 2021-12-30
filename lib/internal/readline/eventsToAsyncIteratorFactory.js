'use strict';
const {
  Error,
  Promise,
  SymbolAsyncIterator,
  ArrayPrototypeConcat,
} = primordials;
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const FixedQueue = require('internal/fixed_queue');

const PAUSE_THRESHOLD = 1024;
const RESUME_THRESHOLD = 1;
const ITEM_EVENTS = ['data'];
const CLOSE_EVENTS = ['close', 'end'];
const ERROR_EVENTS = ['error'];

module.exports = function eventsToAsyncIteratorFactory(readable, {
  pauseThreshold = PAUSE_THRESHOLD,
  resumeThreshold = RESUME_THRESHOLD,
  closeEvents = CLOSE_EVENTS,
  itemEvents = ITEM_EVENTS,
  errorEvents = ERROR_EVENTS,
}) {
  const events = ArrayPrototypeConcat(itemEvents, errorEvents, closeEvents);
  const highWaterMark = RESUME_THRESHOLD;
  let onFunction;
  let offFunction;
  if (typeof readable.on === 'function') {
    onFunction = 'on';
    offFunction = 'off';
  } else if (typeof readable.addEventListener === 'function') {
    onFunction = 'addEventListener';
    offFunction = 'removeEventListener';
  } else {
    throw new ERR_INVALID_ARG_TYPE('readable', 'Readable', readable);
  }

  const queue = new FixedQueue();
  let done = false;
  let error;
  let queueSize = 0;
  let paused = false;
  const onError = (value) => {
    turn(offFunction);
    error = value;
  };
  const onClose = () => {
    turn(offFunction);
    done = true;
  };
  const onItem = (value) => {
    queue.push(value);
    queueSize++;
    if (queueSize >= pauseThreshold) {
      paused = true;
      readable.pause();
    }
  };
  function turn(onOff) {
    for (let i = 0; i < closeEvents.length; i++)
      readable[onOff](closeEvents[i], onClose);
    for (let i = 0; i < itemEvents.length; i++)
      readable[onOff](itemEvents[i], onItem);
    for (let i = 0; i < itemEvents.length; i++)
      readable[onOff](errorEvents[i], onError);
  }

  turn(onFunction);

  function next() {
    if (!queue.isEmpty()) {
      const value = queue.shift();
      queueSize--;
      if (queueSize < resumeThreshold) {
        paused = false;
        readable.resume();
      }
      return {
        done: false,
        value,
      };
    }
    if (error) {
      throw error;
    }
    if (done) {
      return { done };
    }
    return new Promise((resolve, reject) => {
      const resolveNext = () => {
        for (let i = 0; i < events.length; i++)
          readable[offFunction](events[i], resolveNext);
        try {
          resolve(next());
        } catch (promiseError) {
          reject(promiseError);
        }
      };
      for (let i = 0; i < events.length; i++)
        readable[onFunction](events[i], resolveNext);
    });
  }

  return {
    next,
    highWaterMark,
    get isPaused() {
      return paused;
    },
    get queueSize() {
      return queueSize;
    },
    return() {
      turn(offFunction);
      done = true;

      return { done };
    },
    throw(err) {
      if (!err || !(err instanceof Error)) {
        throw new ERR_INVALID_ARG_TYPE('err',
                                       'Error', err);
      }
      error = err;
      turn(offFunction);
    },
    [SymbolAsyncIterator]() { return this; },
  };
};

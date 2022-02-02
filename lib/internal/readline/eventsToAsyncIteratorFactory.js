'use strict';
const {
  ArrayPrototypeConcat,
  Error,
  Promise,
  SymbolAsyncIterator,
} = primordials;
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const FixedQueue = require('internal/fixed_queue');

const kPauseThreshold = 1024;
const kResumeThreshold = 1;
const kItemEvents = ['data'];
const kCloseEvents = ['close', 'end'];
const kErrorEvents = ['error'];

module.exports = function eventsToAsyncIteratorFactory(readable, {
  pauseThreshold = kPauseThreshold,
  resumeThreshold = kResumeThreshold,
  closeEvents = kCloseEvents,
  itemEvents = kItemEvents,
  errorEvents = kErrorEvents,
}) {
  const events = ArrayPrototypeConcat(itemEvents, errorEvents, closeEvents);
  const highWaterMark = kResumeThreshold;
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

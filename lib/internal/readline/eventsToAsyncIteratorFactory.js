'use strict';
const {
  Promise,
  SymbolAsyncIterator,
  ArrayPrototypeConcat,
} = primordials;
const FixedQueue = require('internal/fixed_queue');

const PAUSE_THRESHOLD = 1024;
const RESUME_THRESHOLD = 1;
const ITEM_EVENTS = ['data'];
const CLOSE_EVENTS = ['close', 'end'];
const ERROR_EVENTS = ['error'];


function waitNext(emitter, next, events) {
  return new Promise((resolve, reject) => {
    const resolveNext = () => {
      for (let i = 0; i < events.length; i++)
        emitter.off(events[i], resolveNext);
      try {
        resolve(next());
      } catch (promiseError) {
        reject(promiseError);
      }
    };
    for (let i = 0; i < events.length; i++)
      emitter.on(events[i], resolveNext);
  });
}

module.exports = function eventsToAsyncIteratorFactory(readable, {
  pauseThreshold = PAUSE_THRESHOLD,
  resumeThreshold = RESUME_THRESHOLD,
  closeEvents = CLOSE_EVENTS,
  itemEvents = ITEM_EVENTS,
  errorEvents = ERROR_EVENTS,
}) {
  const events = ArrayPrototypeConcat(itemEvents, errorEvents, closeEvents);
  const highWaterMark = RESUME_THRESHOLD;

  const queue = new FixedQueue();
  let done = false;
  let error;
  let queueSize = 0;
  let paused = false;
  const onError = (value) => {
    turn('off');
    error = value;
  };
  const onClose = () => {
    turn('off');
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

  turn('on');

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
    return waitNext(readable, next, events);
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
    [SymbolAsyncIterator]() { return this; },
  };
};

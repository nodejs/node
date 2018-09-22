'use strict';

const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;
const { AsyncResource } = require('async_hooks');
const { getDefaultTriggerAsyncId } = require('internal/async_hooks');
const { enqueueMicrotask } = internalBinding('util');

// declared separately for name, arrow function to prevent construction
const queueMicrotask = (callback) => {
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('callback', 'function', callback);
  }

  const asyncResource = new AsyncResource('Microtask', {
    triggerAsyncId: getDefaultTriggerAsyncId(),
    requireManualDestroy: true,
  });

  enqueueMicrotask(() => {
    asyncResource.runInAsyncScope(() => {
      try {
        callback();
      } catch (e) {
        process.emit('error', e);
      }
    });
    asyncResource.emitDestroy();
  });
};

module.exports = { queueMicrotask };

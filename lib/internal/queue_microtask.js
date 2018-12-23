'use strict';

const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;
const { AsyncResource } = require('async_hooks');
const { getDefaultTriggerAsyncId } = require('internal/async_hooks');
const {
  enqueueMicrotask,
  triggerFatalException
} = internalBinding('util');

function queueMicrotask(callback) {
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
      } catch (error) {
        // TODO(devsnek) remove this if
        // https://bugs.chromium.org/p/v8/issues/detail?id=8326
        // is resolved such that V8 triggers the fatal exception
        // handler for microtasks
        triggerFatalException(error);
      } finally {
        asyncResource.emitDestroy();
      }
    });
  });
}

module.exports = { queueMicrotask };

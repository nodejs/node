'use strict';

// Calling async_hooks.createHook() emits the DEP0212 deprecation warning once.

const common = require('../common');
const { createHook } = require('async_hooks');

common.expectWarning(
  'DeprecationWarning',
  'async_hooks.createHook() is deprecated. Use AsyncLocalStorage for async ' +
  'context tracking and diagnostics_channel for tracing instead.',
  'DEP0212',
);

createHook({ init() {} }).enable().disable();
createHook({ destroy() {} });

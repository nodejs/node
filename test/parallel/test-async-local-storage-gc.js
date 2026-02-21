// Flags: --async-context-frame --expose-gc --expose-internals

'use strict';
const common = require('../common');
const { gcUntil } = require('../common/gc');
const { AsyncLocalStorage } = require('async_hooks');
const AsyncContextFrame = require('internal/async_context_frame');

// To be compatible with `test-without-async-context-frame.mjs`.
if (!AsyncContextFrame.enabled) {
  common.skip('AsyncContextFrame is not enabled');
}

// Verify that the AsyncLocalStorage object can be garbage collected even without
// `asyncLocalStorage.disable()` being called, when `--async-context-frame` is enabled.

let weakRef = null;
{
  const alsValue = {};
  let als = new AsyncLocalStorage();
  als.run(alsValue, () => {
    setInterval(() => {
      /**
       * Empty interval to keep the als value alive.
       */
    }, 1000).unref();
  });
  weakRef = new WeakRef(als);
  als = null;
}

gcUntil('AsyncLocalStorage object can be garbage collected', () => {
  return weakRef.deref() === undefined;
});

'use strict';
// Flags: --expose_gc --expose-internals

// This test ensures that AsyncLocalStorage gets gced once it was disabled
// and no strong references remain in userland.

const common = require('../common');
const { AsyncLocalStorage } = require('async_hooks');
const AsyncContextFrame = require('internal/async_context_frame');
const { onGC } = require('../common/gc');

let asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run({}, () => {
  asyncLocalStorage.disable();

  onGC(asyncLocalStorage, { ongc: common.mustCall() });
});

if (AsyncContextFrame.enabled) {
  // This disable() is needed to remove reference form AsyncContextFrame
  // created during exit of run() to the AsyncLocalStore instance.
  asyncLocalStorage.disable();
}

asyncLocalStorage = null;
global.gc();

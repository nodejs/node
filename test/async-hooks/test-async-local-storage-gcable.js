'use strict';
// Flags: --expose_gc

// This test ensures that AsyncLocalStorage gets gced once it was disabled
// and no strong references remain in userland.

const common = require('../common');
const { AsyncLocalStorage } = require('async_hooks');
const { onGC } = require('../common/gc');

let asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run({}, common.mustCall(() => {
  asyncLocalStorage.disable();

  onGC(asyncLocalStorage, { ongc: common.mustCall() });
}));

// This disable() is needed to remove reference from AsyncContextFrame
// created during exit of run() to the AsyncLocalStore instance.
asyncLocalStorage.disable();

asyncLocalStorage = null;
global.gc();

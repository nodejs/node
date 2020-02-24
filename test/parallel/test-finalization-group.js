'use strict';

// Flags: --expose-gc --harmony-weak-refs

const common = require('../common');

const g = new globalThis.FinalizationRegistry(common.mustCallAtLeast(1));
g.register({}, 42);

setTimeout(() => {
  globalThis.gc();
  g.cleanupSome();
}, 200);

'use strict';

// Flags: --expose-gc --harmony-weak-refs --harmony-weak-refs-with-cleanup-some

const common = require('../common');

const g = new globalThis.FinalizationRegistry(common.mustCallAtLeast(1));
g.register({}, 42);

setTimeout(() => {
  globalThis.gc();
  g.cleanupSome();
}, 200);

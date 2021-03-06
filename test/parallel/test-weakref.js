'use strict';

// Flags: --expose-gc --harmony-weak-refs

require('../common');
const assert = require('assert');

const w = new globalThis.WeakRef({});

setTimeout(() => {
  globalThis.gc();
  assert.strictEqual(w.deref(), undefined);
}, 200);

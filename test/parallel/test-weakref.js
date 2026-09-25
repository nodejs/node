'use strict';

// Flags: --expose-gc

const common = require('../common');
const assert = require('assert');

const w = new globalThis.WeakRef({});

setTimeout(common.mustCall(() => {
  globalThis.gc();
  assert.strictEqual(w.deref(), undefined);
}), 200);

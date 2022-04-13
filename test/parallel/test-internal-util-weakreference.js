// Flags: --expose-internals --expose-gc
'use strict';
require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { WeakReference } = internalBinding('util');

let obj = { hello: 'world' };
const ref = new WeakReference(obj);
assert.strictEqual(ref.get(), obj);

setImmediate(() => {
  obj = null;
  global.gc();

  assert.strictEqual(ref.get(), undefined);
});

'use strict';

const { internalBinding } = require('internal/test/binding');
const { WeakReference } = internalBinding('util');
const {
  setDeserializeMainFunction
} = require('v8').startupSnapshot
const assert = require('assert');

let obj = { hello: 'world' };
const ref = new WeakReference(obj);

setDeserializeMainFunction(() => {
  obj = null;
  globalThis.gc();

  setImmediate(() => {
    assert.strictEqual(ref.get(), undefined);
  });
});

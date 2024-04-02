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
  assert.strictEqual(ref.get(), obj);
});

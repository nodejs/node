'use strict';

const { WeakReference } = require('internal/util');
const {
  setDeserializeMainFunction
} = require('v8').startupSnapshot
const assert = require('assert');

let obj = { hello: 'world' };
const ref = new WeakReference(obj);

setDeserializeMainFunction(() => {
  assert.strictEqual(ref.get(), obj);
});

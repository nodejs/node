// Flags: --expose-gc

'use strict';
const common = require('../../common');
const { gcUntil } = require('../../common/gc');
const assert = require('assert');
const addon = require(`./build/${common.buildType}/nested_wrap`);

// This test verifies that ObjectWrap and napi_ref can be nested and finalized
// correctly with a non-basic finalizer.
(() => {
  let obj = new addon.NestedWrap();
  obj = null;
  // Silent eslint about unused variables.
  assert.strictEqual(obj, null);
})();

gcUntil('object-wrap-ref', () => {
  return addon.getFinalizerCallCount() === 1;
});

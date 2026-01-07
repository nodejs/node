'use strict';
// Flags: --expose-gc
// Addons: nested_wrap, nested_wrap_vtable

const { addonPath } = require('../../common/addon-test');
const { gcUntil } = require('../../common/gc');
const assert = require('assert');
const addon = require(addonPath);

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

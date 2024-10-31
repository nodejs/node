// Flags: --expose-gc

'use strict';
const common = require('../../common');
const addon = require(`./build/${common.buildType}/6_object_wrap_basic_finalizer`);
const { onGC } = require('../../common/gc');

/**
 * This test verifies that an ObjectWrap can be correctly finalized with an NAPI_EXPERIMENTAL
 * node_api_basic_finalizer.
 */

{
  let obj = new addon.MyObject(9);
  onGC(obj, {
    ongc: common.mustCall(),
  });
  obj = null;
  global.gc();
}

// Flags: --expose-gc

'use strict';
const common = require('../../common');
const addon = require(`./build/${common.buildType}/6_object_wrap`);

(function scope() {
  addon.objectWrapDanglingReference({});
})();

common.gcUntil('object-wrap-ref', () => {
  return addon.objectWrapDanglingReferenceTest();
});

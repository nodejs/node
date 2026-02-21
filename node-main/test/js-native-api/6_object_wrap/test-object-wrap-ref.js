// Flags: --expose-gc

'use strict';
const common = require('../../common');
const addon = require(`./build/${common.buildType}/myobject`);
const { gcUntil } = require('../../common/gc');

(function scope() {
  addon.objectWrapDanglingReference({});
})();

gcUntil('object-wrap-ref', () => {
  return addon.objectWrapDanglingReferenceTest();
});

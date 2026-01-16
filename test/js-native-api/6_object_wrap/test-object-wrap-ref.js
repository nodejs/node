'use strict';
// Flags: --expose-gc
// Addons: myobject, myobject_vtable

const { addonPath } = require('../../common/addon-test');
const addon = require(addonPath);
const { gcUntil } = require('../../common/gc');

(function scope() {
  addon.objectWrapDanglingReference({});
})();

gcUntil('object-wrap-ref', () => {
  return addon.objectWrapDanglingReferenceTest();
});

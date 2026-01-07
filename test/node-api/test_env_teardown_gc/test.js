'use strict';
// Flags: --expose-gc
// Addons: binding, binding_vtable

process.env.NODE_TEST_KNOWN_GLOBALS = 0;

const { addonPath } = require('../../common/addon-test');
const binding = require(addonPath);

global.it = new binding.MyObject();

global.cleanup = () => {
  delete global.it;
  global.gc();
};

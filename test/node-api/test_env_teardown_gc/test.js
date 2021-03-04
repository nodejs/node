'use strict';
// Flags: --expose-gc

process.env.NODE_TEST_KNOWN_GLOBALS = 0;

const common = require('../../common');
const binding = require(`./build/${common.buildType}/binding`);

global.it = new binding.MyObject();

global.cleanup = () => {
  delete global.it;
  global.gc();
};

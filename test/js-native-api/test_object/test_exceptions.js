'use strict';
// Addons: test_exceptions, test_exceptions_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');

// Test
const { testExceptions } = require(addonPath);

function throws() {
  throw new Error('foobar');
}
testExceptions(new Proxy({}, {
  get: common.mustCallAtLeast(throws, 1),
  getOwnPropertyDescriptor: common.mustCallAtLeast(throws, 1),
  defineProperty: common.mustCallAtLeast(throws, 1),
  deleteProperty: common.mustCallAtLeast(throws, 1),
  has: common.mustCallAtLeast(throws, 1),
  set: common.mustCallAtLeast(throws, 1),
  ownKeys: common.mustCallAtLeast(throws, 1),
}));

'use strict';
const common = require('../../common');

// Test
const { testExceptions } = require(`./build/${common.buildType}/test_exceptions`);

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

'use strict';

require('../common');
const constants = process.binding('constants');
const assert = require('assert');

assert.deepStrictEqual(
  Object.keys(constants).sort(), ['crypto', 'fs', 'os', 'trace', 'zlib']
);

assert.deepStrictEqual(
  Object.keys(constants.os).sort(), ['UV_UDP_REUSEADDR', 'dlopen', 'errno',
                                     'priority', 'signals']
);

// Make sure all the constants objects don't inherit from Object.prototype
const inheritedProperties = Object.getOwnPropertyNames(Object.prototype);
function test(obj) {
  assert(obj);
  assert.strictEqual(Object.prototype.toString.call(obj), '[object Object]');
  assert.strictEqual(Object.getPrototypeOf(obj), null);

  inheritedProperties.forEach((property) => {
    assert.strictEqual(property in obj, false);
  });
}

[
  constants, constants.crypto, constants.fs, constants.os, constants.trace,
  constants.zlib, constants.os.dlopen, constants.os.errno, constants.os.signals
].forEach(test);

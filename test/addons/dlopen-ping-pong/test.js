'use strict';
const common = require('../../common');

if (common.isWindows)
  common.skip('dlopen global symbol loading is not supported on this os.');

const assert = require('assert');
const path = require('path');
const os = require('os');

const bindingPath = require.resolve(`./build/${common.buildType}/binding`);
process.dlopen(module, bindingPath,
               os.constants.dlopen.RTLD_NOW | os.constants.dlopen.RTLD_GLOBAL);
module.exports.load(`${path.dirname(bindingPath)}/ping.so`);
assert.strictEqual(module.exports.ping(), 'pong');

// Check that after the addon is loaded with
// process.dlopen() a require() call fails.
const re = /^Error: Module did not self-register\.$/;
assert.throws(() => require(`./build/${common.buildType}/binding`), re);

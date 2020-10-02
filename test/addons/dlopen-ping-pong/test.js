'use strict';
const common = require('../../common');

if (common.isWindows)
  common.skip('dlopen global symbol loading is not supported on this os.');

const assert = require('assert');
const path = require('path');
const os = require('os');

const bindingPath = require.resolve(`./build/${common.buildType}/binding`);
console.log('process.dlopen:', bindingPath);
process.dlopen(module, bindingPath,
               os.constants.dlopen.RTLD_NOW | os.constants.dlopen.RTLD_GLOBAL);

let pingSOPath = `${path.dirname(bindingPath)}/lib.target/ping.so`;

if (common.isOSX) {
  pingSOPath = `${path.dirname(bindingPath)}/ping.so`;
}

console.log('module.exports.load:', pingSOPath);
module.exports.load(pingSOPath);

assert.strictEqual(module.exports.ping(), 'pong');

// Check that after the addon is loaded with
// process.dlopen() a require() call fails.
console.log('require:', `./build/${common.buildType}/binding`);
const re = /^Error: Module did not self-register: '.*[\\/]binding\.node'\.$/;
assert.throws(() => require(`./build/${common.buildType}/binding`), re);

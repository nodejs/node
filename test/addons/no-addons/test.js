// Flags: --no-addons

'use strict';

const common = require('../../common');
const assert = require('assert');

const bindingPath = require.resolve(`./build/${common.buildType}/binding`);

assert.throws(() => require(bindingPath), {
  code: 'ERR_DLOPEN_DISABLED',
  message: 'Cannot load native addon because loading addons is disabled.',
});
assert.throws(() => process.dlopen({ exports: {} }, bindingPath), {
  code: 'ERR_DLOPEN_DISABLED',
  message: 'Cannot load native addon because loading addons is disabled.',
});

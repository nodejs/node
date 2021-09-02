'use strict';

const common = require('../../common');
const assert = require('assert');

const bindingPath = require.resolve(`./build/${common.buildType}/binding`);

let threw = false;

try {
  require(bindingPath);
} catch (error) {
  threw = true;
  assert(error instanceof Error);
  assert.strictEqual(error.code, 'ERR_DLOPEN_FAILED');
  assert.strictEqual(
    error.message,
    'Cannot load native addon because loading addons is disabled.'
  );
}

assert(threw);

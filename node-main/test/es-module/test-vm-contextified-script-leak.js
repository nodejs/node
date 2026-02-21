// Flags: --max-old-space-size=16 --trace-gc
'use strict';

// This tests that vm.Script with dynamic import callback does not leak.
// See: https://github.com/nodejs/node/issues/33439
require('../common');
const { checkIfCollectable } = require('../common/gc');
const vm = require('vm');

async function createContextifyScript() {
  // Try to reach the maximum old space size.
  return new vm.Script(`"${Math.random().toString().repeat(512)}";`, {
    async importModuleDynamically() {},
  });
}
checkIfCollectable(createContextifyScript, 2048, 512);

// Flags: --max-old-space-size=16 --trace-gc
'use strict';

// This tests that vm.compileFunction with dynamic import callback does not leak.
// See https://github.com/nodejs/node/issues/44211
require('../common');
const { checkIfCollectable } = require('../common/gc');
const vm = require('vm');

async function createCompiledFunction() {
  return vm.compileFunction(`"${Math.random().toString().repeat(512)}"`, [], {
    async importModuleDynamically() {},
  });
}

checkIfCollectable(createCompiledFunction, 2048);

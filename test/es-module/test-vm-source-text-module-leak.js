// Flags: --experimental-vm-modules --max-old-space-size=16 --trace-gc
'use strict';

// This tests that vm.SourceTextModule() does not leak.
// See: https://github.com/nodejs/node/issues/33439
require('../common');
const { checkIfCollectable } = require('../common/gc');
const vm = require('vm');

async function createSourceTextModule() {
  // Try to reach the maximum old space size.
  const m = new vm.SourceTextModule(`
    const bar = new Array(512).fill("----");
    export { bar };
  `);
  await m.link(() => {});
  await m.evaluate();
  return m;
}

checkIfCollectable(createSourceTextModule, 4096, 1024);

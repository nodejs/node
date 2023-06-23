// Flags: --experimental-vm-modules --max-old-space-size=16 --trace-gc
'use strict';

// This tests that vm.SourceTextModule() does not leak.
// See: https://github.com/nodejs/node/issues/33439
require('../common');

const vm = require('vm');
let count = 0;
async function createModule() {
  // Try to reach the maximum old space size.
  const m = new vm.SourceTextModule(`
  const bar = new Array(512).fill("----");
  export { bar };
`);
  await m.link(() => {});
  await m.evaluate();
  if (count++ < 4096) {
    setTimeout(createModule, 1);
  }
  return m;
}
createModule();

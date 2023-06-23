// Flags: --max-old-space-size=16 --trace-gc
'use strict';

// This tests that vm.Script with dynamic import callback does not leak.
// See: https://github.com/nodejs/node/issues/33439
require('../common');
const vm = require('vm');
let count = 0;

function main() {
  // Try to reach the maximum old space size.
  new vm.Script(`"${Math.random().toString().repeat(512)}";`, {
    async importModuleDynamically() {},
  });
  if (count++ < 2 * 1024) {
    setTimeout(main, 1);
  }
}
main();

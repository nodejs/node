// Flags: --experimental-vm-modules --max-old-space-size=16
'use strict';

// This tests that vm.SyntheticModule does not leak.
// See https://github.com/nodejs/node/issues/44211
require('../common');
const vm = require('vm');

let count = 0;
async function createModule() {
  // Try to reach the maximum old space size.
  const m = new vm.SyntheticModule(['bar'], () => {
    m.setExport('bar', new Array(512).fill('----'));
  });
  await m.link(() => {});
  await m.evaluate();
  if (count++ < 4 * 1024) {
    setTimeout(createModule, 1);
  }
  return m;
}

createModule();

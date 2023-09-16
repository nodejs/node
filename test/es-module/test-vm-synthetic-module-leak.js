// Flags: --experimental-vm-modules --max-old-space-size=16
'use strict';

// This tests that vm.SyntheticModule does not leak.
// See https://github.com/nodejs/node/issues/44211
require('../common');
const { checkIfCollectable } = require('../common/gc');
const vm = require('vm');

async function createSyntheticModule() {
  const m = new vm.SyntheticModule(['bar'], () => {
    m.setExport('bar', new Array(512).fill('----'));
  });
  await m.link(() => {});
  await m.evaluate();
  return m;
}
checkIfCollectable(createSyntheticModule, 4096);

// Flags: --expose-gc --experimental-vm-modules

'use strict';

// This tests that vm.Script would not get GC'ed while the script can still
// initiate dynamic import.
// See https://github.com/nodejs/node/issues/43205.

require('../common');
const vm = require('vm');

const code = `
new Promise(resolve => {
  setTimeout(() => {
    gc();  // vm.Script should not be GC'ed while the script is alive.
    resolve();
  }, 1);
}).then(() => import('foo'));`;

// vm.runInThisContext creates a vm.Script underneath, which should not be GC'ed
// while import() can still be initiated.
vm.runInThisContext(code, {
  async importModuleDynamically() {
    const m = new vm.SyntheticModule(['bar'], () => {
      m.setExport('bar', 1);
    });

    await m.link(() => {});
    await m.evaluate();
    return m;
  }
});

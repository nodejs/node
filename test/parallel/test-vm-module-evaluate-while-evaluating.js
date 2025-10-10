// Flags: --experimental-vm-modules
'use strict';

// This tests the result of evaluating a vm.Module while it is evaluating.
const common = require('../common');

const assert = require('assert');
const vm = require('vm');

{
  let mod;
  globalThis.evaluate = common.mustCall(() => {
    assert.throws(() => { mod.evaluate(); }, {
      code: 'ERR_VM_MODULE_STATUS'
    });
  });
  common.allowGlobals(globalThis.evaluate);
  mod = new vm.SourceTextModule(`
    globalThis.evaluate();
    export const a = 42;
  `);
  mod.linkRequests([]);
  mod.instantiate();
  mod.evaluate();
}

{
  const mod = new vm.SyntheticModule(['a'], common.mustCall(() => {
    assert.throws(() => { mod.evaluate(); }, {
      code: 'ERR_VM_MODULE_STATUS'
    });
  }));
  mod.evaluate();
}

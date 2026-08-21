'use strict';

const common = require('../common');
const { Script, compileFunction } = require('vm');
const assert = require('assert');

assert(
  !process.execArgv.includes('--experimental-vm-modules'),
  'This test must be run without --experimental-vm-modules');

assert.rejects(async () => {
  const script = new Script('import("fs")', {
    importModuleDynamically: common.mustNotCall(),
  });
  const imported = script.runInThisContext();
  await imported;
}, {
  code: 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING_FLAG'
}).then(common.mustCall());

assert.rejects(async () => {
  const imported = compileFunction('return import("fs")', [], {
    importModuleDynamically: common.mustNotCall(),
  })();
  await imported;
}, {
  code: 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING_FLAG'
}).then(common.mustCall());

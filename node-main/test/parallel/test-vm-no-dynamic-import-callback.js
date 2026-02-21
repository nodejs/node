'use strict';

const common = require('../common');
const { Script, compileFunction } = require('vm');
const assert = require('assert');

assert.rejects(async () => {
  const script = new Script('import("fs")');
  const imported = script.runInThisContext();
  await imported;
}, {
  code: 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING'
}).then(common.mustCall());

assert.rejects(async () => {
  const imported = compileFunction('return import("fs")')();
  await imported;
}, {
  code: 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING'
}).then(common.mustCall());

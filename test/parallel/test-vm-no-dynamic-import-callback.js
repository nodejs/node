'use strict';

require('../common');
const { Script } = require('vm');
const assert = require('assert');

assert.rejects(async () => {
  const script = new Script('import("fs")');
  const imported = script.runInThisContext();
  await imported;
}, {
  code: 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING'
});

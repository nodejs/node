'use strict';
require('../common');

const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');

const script = `
Object.defineProperty(Array.prototype, '2', {
  __proto__: null,
  set() {},
  configurable: true,
});

try {
  require('child_process').spawn(process.execPath, ['-e', '0']);
  console.log('NO_ERROR');
} catch (error) {
  console.log(error.code);
}
`;

spawnSyncAndAssert(process.execPath, ['-e', script], {
  stdout: (output) => {
    assert.match(output, /^ERR_INVALID_ARG_TYPE\\r?\\n$/);
  },
  stderr: (output) => {
    assert.doesNotMatch(output, /FATAL ERROR: v8::ToLocalChecked Empty MaybeLocal/);
  },
});

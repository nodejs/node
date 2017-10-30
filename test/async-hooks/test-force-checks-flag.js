'use strict';
const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const spawnSync = require('child_process').spawnSync;

// disabling this way will just decrement the kCheck counter. It won't actually
// disable the checks, because they were enabled with the flag.
common.revert_force_async_hooks_checks();

switch (process.argv[2]) {
  case 'test_invalid_async_id':
    async_hooks.emitInit();
    return;
}
assert.ok(!process.argv[2]);


const c1 = spawnSync(process.execPath, [
  '--force-async-hooks-checks', __filename, 'test_invalid_async_id'
]);
assert.strictEqual(
  c1.stderr.toString().split(/[\r\n]+/g)[0],
  'RangeError [ERR_INVALID_ASYNC_ID]: Invalid asyncId value: undefined');
assert.strictEqual(c1.status, 1);

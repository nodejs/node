'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

// This test spawns itself with an argument to indicate when it is a child to
// easily and portably print the value of argv[0]
if (process.argv[2] === 'child') {
  console.log(process.argv0);
  return;
}

const noArgv0 = cp.spawnSync(process.execPath, [__filename, 'child']);
assert.strictEqual(noArgv0.stdout.toString().trim(), process.execPath);

const withArgv0 = cp.spawnSync(process.execPath, [__filename, 'child'],
                               { argv0: 'withArgv0' });
assert.strictEqual(withArgv0.stdout.toString().trim(), 'withArgv0');

assert.throws(() => {
  cp.spawnSync(process.execPath, [__filename, 'child'], { argv0: [] });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: 'The "options.argv0" property must be of type string.' +
           common.invalidArgTypeHelper([])
});

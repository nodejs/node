'use strict';
const common = require('../common');
const assert = require('assert');

const spawnSync = require('child_process').spawnSync;

// Echo does different things on Windows and Unix, but in both cases, it does
// more-or-less nothing if there are no parameters
const ret = spawnSync('sleep', ['0']);
assert.strictEqual(ret.status, 0, 'exit status should be zero');

// Error test when command does not exist
const ret_err = spawnSync('command_does_not_exist', ['bar']).error;

assert.strictEqual(ret_err.code, 'ENOENT');
assert.strictEqual(ret_err.errno, 'ENOENT');
assert.strictEqual(ret_err.syscall, 'spawnSync command_does_not_exist');
assert.strictEqual(ret_err.path, 'command_does_not_exist');
assert.deepEqual(ret_err.spawnargs, ['bar']);

// Verify that the cwd option works - GH #7824
(function() {
  var response;
  var cwd;

  if (common.isWindows) {
    cwd = 'c:\\';
    response = spawnSync('cmd.exe', ['/c', 'cd'], {cwd: cwd});
  } else {
    cwd = '/';
    response = spawnSync('pwd', [], {cwd: cwd});
  }

  assert.strictEqual(response.stdout.toString().trim(), cwd);
})();

'use strict';
var common = require('../common');
var assert = require('assert');

var spawnSync = require('child_process').spawnSync;

var TIMER = 100;
var SLEEP = 1000;

var timeout = 0;

setTimeout(function() {
  timeout = process.hrtime(start);
  assert.ok(stop, 'timer should not fire before process exits');
  assert.strictEqual(timeout[0], 1, 'timer should take as long as sleep');
}, TIMER);

console.log('sleep started');
var start = process.hrtime();
var ret = spawnSync('sleep', ['1']);
var stop = process.hrtime(start);
assert.strictEqual(ret.status, 0, 'exit status should be zero');
console.log('sleep exited', stop);
assert.strictEqual(stop[0], 1,
                   'sleep should not take longer or less than 1 second');

// Error test when command does not exist
var ret_err = spawnSync('command_does_not_exist', ['bar']).error;

assert.strictEqual(ret_err.code, 'ENOENT');
assert.strictEqual(ret_err.errno, 'ENOENT');
assert.strictEqual(ret_err.syscall, 'spawnSync command_does_not_exist');
assert.strictEqual(ret_err.path, 'command_does_not_exist');
assert.deepEqual(ret_err.spawnargs, ['bar']);

// Verify that the cwd option works - GH #7824
(function() {
  var response;
  var cwd;

  if (process.platform === 'win32') {
    cwd = 'c:\\';
    response = spawnSync('cmd.exe', ['/c', 'cd'], {cwd: cwd});
  } else {
    cwd = '/';
    response = spawnSync('pwd', [], {cwd: cwd});
  }

  assert.strictEqual(response.stdout.toString().trim(), cwd);
})();

'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var path = require('path');

var returns = 0;

/*
  Spawns 'pwd' with given options, then test
  - whether the exit code equals forCode,
  - optionally whether the stdout result matches forData
    (after removing traling whitespace)
*/
function testCwd(options, forCode, forData) {
  var data = '';

  var child = common.spawnPwd(options);

  child.stdout.setEncoding('utf8');

  child.stdout.on('data', function(chunk) {
    data += chunk;
  });

  child.on('exit', function(code, signal) {
    assert.strictEqual(forCode, code);
  });

  child.on('close', function() {
    forData && assert.strictEqual(forData, data.replace(/[\s\r\n]+$/, ''));
    returns--;
  });

  returns++;

  return child;
}

// Assume these exist, and 'pwd' gives us the right directory back
if (process.platform == 'win32') {
  testCwd({cwd: process.env.windir}, 0, process.env.windir);
  testCwd({cwd: 'c:\\'}, 0, 'c:\\');
} else {
  testCwd({cwd: '/dev'}, 0, '/dev');
  testCwd({cwd: '/'}, 0, '/');
}

// Assume does-not-exist doesn't exist, expect exitCode=-1 and errno=ENOENT
(function() {
  var errors = 0;

  testCwd({cwd: 'does-not-exist'}, -1).on('error', function(e) {
    assert.equal(e.code, 'ENOENT');
    errors++;
  });

  process.on('exit', function() {
    assert.equal(errors, 1);
  });
})();

// Spawn() shouldn't try to chdir() so this should just work
testCwd(undefined, 0);
testCwd({}, 0);
testCwd({cwd: ''}, 0);
testCwd({cwd: undefined}, 0);
testCwd({cwd: null}, 0);

// Check whether all tests actually returned
assert.notEqual(0, returns);
process.on('exit', function() {
  assert.equal(0, returns);
});

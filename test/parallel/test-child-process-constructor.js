'use strict';

require('../common');
var assert = require('assert');
var child_process = require('child_process');
var ChildProcess = child_process.ChildProcess;
assert.equal(typeof ChildProcess, 'function');

// test that we can call spawn
var child = new ChildProcess();
child.spawn({
  file: process.execPath,
  args: ['--interactive'],
  cwd: process.cwd(),
  stdio: 'pipe'
});

assert.equal(child.hasOwnProperty('pid'), true);

// try killing with invalid signal
assert.throws(function() {
  child.kill('foo');
}, /Unknown signal: foo/);

assert.equal(child.kill(), true);

'use strict';

require('../common');
const assert = require('assert');
const child_process = require('child_process');
const ChildProcess = child_process.ChildProcess;
assert.strictEqual(typeof ChildProcess, 'function');

// test that we can call spawn
const child = new ChildProcess();
child.spawn({
  file: process.execPath,
  args: ['--interactive'],
  cwd: process.cwd(),
  stdio: 'pipe'
});

assert.strictEqual(child.hasOwnProperty('pid'), true);

// try killing with invalid signal
assert.throws(function() {
  child.kill('foo');
}, /Unknown signal: foo/);

assert.strictEqual(child.kill(), true);

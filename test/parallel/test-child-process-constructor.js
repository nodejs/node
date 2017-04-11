'use strict';

require('../common');
const assert = require('assert');
const { ChildProcess } = require('child_process');
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
assert(Number.isInteger(child.pid));

// try killing with invalid signal
assert.throws(() => {
  child.kill('foo');
}, /^Error: Unknown signal: foo$/);

assert.strictEqual(child.kill(), true);

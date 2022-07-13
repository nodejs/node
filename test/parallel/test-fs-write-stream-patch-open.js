'use strict';
const common = require('../common');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

// Run in a child process because 'out' is opened twice, blocking the tmpdir
// and preventing cleanup.
if (process.argv[2] !== 'child') {
  // Parent
  const assert = require('assert');
  const { fork } = require('child_process');
  tmpdir.refresh();

  // Run test
  const child = fork(__filename, ['child'], { stdio: 'inherit' });
  child.on('exit', common.mustCall(function(code) {
    assert.strictEqual(code, 0);
  }));

  return;
}

// Child

common.expectWarning(
  'DeprecationWarning',
  'WriteStream.prototype.open() is deprecated', 'DEP0135');
const s = fs.createWriteStream(`${tmpdir.path}/out`);
s.open();

process.nextTick(() => {
  // Allow overriding open().
  fs.WriteStream.prototype.open = common.mustCall();
  fs.createWriteStream('asd');
});

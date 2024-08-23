'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');

//
// This test ensures Node.js doesn't crash when reading from /dev/stdin as an input.
// Ref: https://github.com/nodejs/node/issues/54200
//

if (!fs.existsSync('/dev/stdin')) {
  common.skip('Only test on platforms having /dev/stdin');
}

const child_process = require('child_process');
const assert = require('assert');
const { describe, it } = require('node:test');

describe('Test reading SourceCode from stdin and it\'s symlinks', () => {
  it('Test reading sourcecode from /dev/stdin', () => {
    const cp = child_process.execSync(`printf 'console.log(1)' | "${process.execPath}" /dev/stdin`, { stdio: 'pipe' });
    assert.strictEqual(cp.toString(), '1\n');
  });

  it('Test reading sourcecode from a symlink to /dev/stdin', () => {
    tmpdir.refresh();
    const tmp = tmpdir.resolve('./stdin');
    fs.symlinkSync('/dev/stdin', tmp);
    const cp2 = child_process.execSync(`printf 'console.log(2)' | "${process.execPath}" ${tmp}`, { stdio: 'pipe' });
    assert.strictEqual(cp2.toString(), '2\n');
  });

  it('Test reading sourcecode from a symlink to the `readlink /dev/stdin`', () => {
    const devStdin = fs.readlinkSync('/dev/stdin');
    const cp3 = child_process.execSync(`printf 'console.log(3)' | "${process.execPath}" "${devStdin}"`, { stdio: 'pipe' });
    assert.strictEqual(cp3.toString(), '3\n');
  });
});

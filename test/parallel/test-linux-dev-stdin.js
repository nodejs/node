'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');

//
// This test ensures Node.js doesn't crash when reading from /dev/stdin as an input.
// ref: https://github.com/nodejs/node/issues/54200
//

if (!fs.existsSync('/dev/stdin')) {
  common.skip('Only test on platforms having /dev/stdin');
}

const child_process = require('child_process');
const assert = require('assert');

const cp = child_process.execSync(`printf 'console.log(1)' | "${process.execPath}" /dev/stdin`, { stdio: 'pipe' });
assert.strictEqual(cp.toString(), '1\n');

tmpdir.refresh();
const tmp = tmpdir.resolve('./stdin');
fs.symlinkSync('/dev/stdin', tmp);
const cp2 = child_process.execSync(`printf 'console.log(2)' | "${process.execPath}" ${tmp}`, { stdio: 'pipe' });
assert.strictEqual(cp2.toString(), '2\n');

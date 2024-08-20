'use strict';
const common = require('../common');

//
// This test ensures Node.js doesn't crash on linux when reading from /dev/stdin in the CLI
// ref: https://github.com/nodejs/node/issues/54200
//

if (!common.isLinux) {
  common.skip('Linux only test');
}

const child_process = require('child_process');
const assert = require('assert');

const cp = child_process.execSync(`printf 'console.log(1)' | "${process.execPath}" /dev/stdin`, { stdio: 'pipe' });
assert.strictEqual(cp.toString(), '1\n');

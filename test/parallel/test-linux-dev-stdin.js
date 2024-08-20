'use strict';
const common = require('../common');

if (!common.isLinux) {
    common.skip('Linux only test');
}

const child_process = require("child_process")
const assert = require('assert');

const cp = child_process.execSync(`printf 'console.log(1)' | "${process.execPath}" /dev/stdin`, {stdio: 'pipe'})
assert.strictEqual(cp.toString(), '1\n');
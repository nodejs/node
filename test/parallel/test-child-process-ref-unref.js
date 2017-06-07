'use strict';
const common = require('../common');
const spawn = require('child_process').spawn;
const cmd = common.isWindows ? 'rundll32' : 'ls';
const assert = require('assert');

const cp = spawn(cmd);
assert.strictEqual(cp.hasOwnProperty('_unref'), true);
assert.strictEqual(cp._unref, false);
cp.unref();
assert.strictEqual(cp._unref, true);
cp.ref();
assert.strictEqual(cp._unref, false);
cp.unref();
assert.strictEqual(cp._unref, true);

'use strict';
const common = require('../common');
const spawn = require('child_process').spawn;
const cmd = common.isWindows ? 'rundll32' : 'ls';
const assert = require('assert');

const cp = spawn(cmd);
assert.ok(cp.hasOwnProperty('_unref'));
assert.ok(!cp._unref);
cp.unref();
assert.ok(cp._unref);
cp.ref();
assert.ok(!cp._unref);
cp.unref();
assert.ok(cp._unref);

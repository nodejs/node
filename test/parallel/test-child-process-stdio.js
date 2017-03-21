'use strict';
const common = require('../common');
const assert = require('assert');
const spawnSync = require('child_process').spawnSync;

let options = {stdio: ['pipe']};
let child = common.spawnPwd(options);

assert.notStrictEqual(child.stdout, null);
assert.notStrictEqual(child.stderr, null);

options = {stdio: 'ignore'};
child = common.spawnPwd(options);

assert.strictEqual(child.stdout, null);
assert.strictEqual(child.stderr, null);

options = {stdio: 'ignore'};
child = spawnSync('cat', [], options);
assert.deepStrictEqual(options, {stdio: 'ignore'});

assert.throws(() => {
  common.spawnPwd({stdio: ['pipe', 'pipe', 'pipe', 'ipc', 'ipc']});
}, /^Error: Child process can have only one IPC pipe$/);

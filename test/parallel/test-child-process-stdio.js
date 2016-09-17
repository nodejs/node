'use strict';
const common = require('../common');
const assert = require('assert');

let options = {stdio: ['pipe']};
let child = common.spawnPwd(options);

assert.notEqual(child.stdout, null);
assert.notEqual(child.stderr, null);

options = {stdio: 'ignore'};
child = common.spawnPwd(options);

assert.equal(child.stdout, null);
assert.equal(child.stderr, null);

options = {stdio: 'ignore'};
child = common.spawnSyncCat(options);
assert.deepStrictEqual(options, {stdio: 'ignore'});

assert.throws(() => {
  common.spawnPwd({stdio: ['pipe', 'pipe', 'pipe', 'ipc', 'ipc']});
}, /^Error: Child process can have only one IPC pipe$/);

// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const errors = require('internal/errors');

const one_ipc_err = new RegExp(errors.message('CHILD_PROCESS_ONE_IPC'));

let options = {stdio: ['pipe']};
let child = common.spawnPwd(options);

assert.notStrictEqual(child.stdout, null);
assert.notStrictEqual(child.stderr, null);

options = {stdio: 'ignore'};
child = common.spawnPwd(options);

assert.strictEqual(child.stdout, null);
assert.strictEqual(child.stderr, null);

options = {stdio: 'ignore'};
child = common.spawnSyncCat(options);
assert.deepStrictEqual(options, {stdio: 'ignore'});

assert.throws(() => {
  common.spawnPwd({stdio: ['pipe', 'pipe', 'pipe', 'ipc', 'ipc']});
}, one_ipc_err);

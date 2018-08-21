'use strict';
// Refs: https://github.com/nodejs/node/issues/7342
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

const command = process.platform === 'win32' ? 'dir' : 'ls';

exec(command).stdout.on('data', common.mustCallAtLeast());

exec('fhqwhgads').stderr.on('data', common.mustCallAtLeast((data) => {
  assert.strictEqual(typeof data, 'string');
}));

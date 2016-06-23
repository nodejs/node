'use strict';
// Refs: https://github.com/nodejs/node/issues/7342
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

const expectedCalls = 2;

const cb = common.mustCall((data) => {
  assert.strictEqual(typeof data, 'string');
}, expectedCalls);

const command = common.isWindows ? 'dir' : 'ls';
exec(command).stdout.on('data', cb);

exec('fhqwhgads').stderr.on('data', cb);

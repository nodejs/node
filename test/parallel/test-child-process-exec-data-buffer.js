'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

const expectedCalls = 2;

const cb = common.mustCall((data) => {
  assert(data instanceof Buffer);
}, expectedCalls);

const command = common.isWindows ? 'dir' : 'ls';
exec(command).stdout.on('data', cb);

exec('fhqwhgads').stderr.on('data', cb);

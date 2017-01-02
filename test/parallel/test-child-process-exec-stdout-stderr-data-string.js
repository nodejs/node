'use strict';
// Refs: https://github.com/nodejs/node/issues/7342
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

var stdoutCalls = 0;
var stderrCalls = 0;

const command = common.isWindows ? 'dir' : 'ls';
exec(command).stdout.on('data', (data) => {
  stdoutCalls += 1;
});

exec('fhqwhgads').stderr.on('data', (data) => {
  assert.strictEqual(typeof data, 'string');
  stderrCalls += 1;
});

process.on('exit', () => {
  assert(stdoutCalls > 0);
  assert(stderrCalls > 0);
});

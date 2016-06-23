'use strict';
// Refs: https://github.com/nodejs/node/issues/7342
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

var stdoutIsString = false;
var stderrIsString = false;

const command = common.isWindows ? 'dir' : 'ls';
exec(command).stdout.on('data', (data) => {
  stdoutIsString = (typeof data === 'string');
});

exec('fhqwhgads').stderr.on('data', (data) => {
  stderrIsString = (typeof data === 'string');
});

process.on('exit', () => {
  assert(stdoutIsString);
  assert(stderrIsString);
});

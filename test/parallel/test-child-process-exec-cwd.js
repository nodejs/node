'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

var pwdcommand, dir;

if (common.isWindows) {
  pwdcommand = 'echo %cd%';
  dir = 'c:\\windows';
} else {
  pwdcommand = 'pwd';
  dir = '/dev';
}

exec(pwdcommand, {cwd: dir}, common.mustCall(function(err, stdout, stderr) {
  assert.ifError(err);
  assert.strictEqual(stdout.indexOf(dir), 0);
}));

'use strict';
const common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;

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
  assert.ok(stdout.indexOf(dir) == 0);
}));

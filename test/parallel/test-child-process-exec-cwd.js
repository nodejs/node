'use strict';
require('../common');
var assert = require('assert');
var exec = require('child_process').exec;

var success_count = 0;
var error_count = 0;

var pwdcommand, dir;

if (process.platform == 'win32') {
  pwdcommand = 'echo %cd%';
  dir = 'c:\\windows';
} else {
  pwdcommand = 'pwd';
  dir = '/dev';
}

var child = exec(pwdcommand, {cwd: dir}, function(err, stdout, stderr) {
  if (err) {
    error_count++;
    console.log('error!: ' + err.code);
    console.log('stdout: ' + JSON.stringify(stdout));
    console.log('stderr: ' + JSON.stringify(stderr));
    assert.equal(false, err.killed);
  } else {
    success_count++;
    console.log(stdout);
    assert.ok(stdout.indexOf(dir) == 0);
  }
});

process.on('exit', function() {
  assert.equal(1, success_count);
  assert.equal(0, error_count);
});

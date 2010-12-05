require('../common');
var assert = require('assert');
var exec = require('child_process').exec;

var success_count = 0;
var error_count = 0;

var child = exec('pwd', {cwd: '/dev'}, function(err, stdout, stderr) {
  if (err) {
    error_count++;
    console.log('error!: ' + err.code);
    console.log('stdout: ' + JSON.stringify(stdout));
    console.log('stderr: ' + JSON.stringify(stderr));
    assert.equal(false, err.killed);
  } else {
    success_count++;
    assert.equal(true, /^\/dev\b/.test(stdout));
  }
});

process.addListener('exit', function() {
  assert.equal(1, success_count);
  assert.equal(0, error_count);
});

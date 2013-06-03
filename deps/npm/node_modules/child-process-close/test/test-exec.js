
require('../index');

var assert = require('assert'),
    exec = require('child_process').exec;

var cp = exec('echo hello', function(err) {
  assert(!err);
});

var stdoutData = '',
    stdoutEnd = false,
    gotExit = false,
    gotClose = false;

cp.stdout.setEncoding('ascii');

cp.stdout.on('data', function(data) {
  assert(!stdoutEnd);
  stdoutData += data;
});

cp.stdout.on('end', function() {
  assert(!stdoutEnd);
  assert(/^hello/.test(stdoutData));
  stdoutEnd = true;
});

cp.on('exit', function(code, signal) {
  assert.strictEqual(code, 0);
  assert(!signal);
  assert(!gotExit);
  assert(!gotClose);
  gotExit = true;
});

cp.on('close', function(code, signal) {
  assert.strictEqual(code, 0);
  assert(!signal);
  assert(gotExit);
  assert(stdoutEnd);
  assert(!gotClose);
  gotClose = true;
});

process.on('exit', function() {
  assert(stdoutEnd);
  assert(gotExit);
  assert(gotClose);
});

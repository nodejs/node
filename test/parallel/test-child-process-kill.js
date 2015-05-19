'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var is_windows = process.platform === 'win32';

var exitCode;
var termSignal;
var gotStdoutEOF = false;
var gotStderrEOF = false;

var cat = spawn(is_windows ? 'cmd' : 'cat');


cat.stdout.on('end', function() {
  gotStdoutEOF = true;
});

cat.stderr.on('data', function(chunk) {
  assert.ok(false);
});

cat.stderr.on('end', function() {
  gotStderrEOF = true;
});

cat.on('exit', function(code, signal) {
  exitCode = code;
  termSignal = signal;
});

assert.equal(cat.killed, false);
cat.kill();
assert.equal(cat.killed, true);

process.on('exit', function() {
  assert.strictEqual(exitCode, null);
  assert.strictEqual(termSignal, 'SIGTERM');
  assert.ok(gotStdoutEOF);
  assert.ok(gotStderrEOF);
});

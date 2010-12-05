var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var exitCode;
var termSignal;
var gotStdoutEOF = false;
var gotStderrEOF = false;

var cat = spawn('cat');


cat.stdout.addListener('data', function(chunk) {
  assert.ok(false);
});

cat.stdout.addListener('end', function() {
  gotStdoutEOF = true;
});

cat.stderr.addListener('data', function(chunk) {
  assert.ok(false);
});

cat.stderr.addListener('end', function() {
  gotStderrEOF = true;
});

cat.addListener('exit', function(code, signal) {
  exitCode = code;
  termSignal = signal;
});

cat.kill();

process.addListener('exit', function() {
  assert.strictEqual(exitCode, null);
  assert.strictEqual(termSignal, 'SIGTERM');
  assert.ok(gotStdoutEOF);
  assert.ok(gotStderrEOF);
});

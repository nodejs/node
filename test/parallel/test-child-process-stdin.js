'use strict';
var common = require('../common');
var assert = require('assert');
const path = require('path');
var spawn = require('child_process').spawn;
const exec = require('child_process').exec;

var cat = spawn(common.isWindows ? 'more' : 'cat');
cat.stdin.write('hello');
cat.stdin.write(' ');
cat.stdin.write('world');

assert.ok(cat.stdin.writable);
assert.ok(!cat.stdin.readable);

cat.stdin.end();

var response = '';
var exitStatus = -1;
var closed = false;

var gotStdoutEOF = false;

cat.stdout.setEncoding('utf8');
cat.stdout.on('data', function(chunk) {
  console.log('stdout: ' + chunk);
  response += chunk;
});

cat.stdout.on('end', function() {
  gotStdoutEOF = true;
});


var gotStderrEOF = false;

cat.stderr.on('data', function(chunk) {
  // shouldn't get any stderr output
  assert.ok(false);
});

cat.stderr.on('end', function(chunk) {
  gotStderrEOF = true;
});


cat.on('exit', function(status) {
  console.log('exit event');
  exitStatus = status;
});

cat.on('close', function() {
  closed = true;
  if (common.isWindows) {
    assert.equal('hello world\r\n', response);
  } else {
    assert.equal('hello world', response);
  }
});

process.on('exit', function() {
  assert.equal(0, exitStatus);
  assert(closed);
  if (common.isWindows) {
    assert.equal('hello world\r\n', response);
  } else {
    assert.equal('hello world', response);
  }
});

// Regression test for https://github.com/nodejs/io.js/issues/2333
const cpFile = path.join(common.fixturesDir, 'child-process-stdin.js');
const nodeBinary = process.argv[0];

exec(`${nodeBinary} ${cpFile}`, function(err, stdout, stderr) {
  const stdoutLines = stdout.split('\n');
  assert.strictEqual(stdoutLines[0], 'true');
  assert.strictEqual(stdoutLines[1], 'false');
  assert.strictEqual(stdoutLines.length, 3);

  const stderrLines = stderr.split('\n');
  assert.strictEqual(stderrLines[0], '[Error: Not a raw device]');
  assert.strictEqual(stderrLines.length, 2);
});

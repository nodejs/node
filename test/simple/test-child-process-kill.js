// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var exitCode;
var termSignal;
var gotStdoutEOF = false;
var gotStderrEOF = false;

var ping = "42\n";

var cat = spawn('cat');

//
// This test sends a signal to a child process.
//
// There is a potential race here where the signal is delivered
// after the fork() but before execve(). IOW, the signal is sent
// before the child process has truly been started.
//
// So we wait for a sign of life from the child (the ping response)
// before sending the signal.
//
cat.stdin.write(ping);

cat.stdout.addListener('data', function(chunk) {
  assert.equal(chunk.toString(), ping);
  cat.kill();
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

process.addListener('exit', function() {
  assert.strictEqual(exitCode, null);
  assert.strictEqual(termSignal, 'SIGTERM');
  assert.ok(gotStdoutEOF);
  assert.ok(gotStderrEOF);
});

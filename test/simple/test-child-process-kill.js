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

var is_windows = process.platform === 'win32';

var exitCode;
var termSignal;
var gotStdoutEOF = false;
var gotStderrEOF = false;

var cat = spawn(is_windows ? 'cmd' : 'cat');


cat.stdout.on('data', function(chunk) {
  assert.ok(false);
});

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

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

// This test is to assert that we can SIGINT a script which loops forever.
// Ref(http):
// groups.google.com/group/nodejs-dev/browse_thread/thread/e20f2f8df0296d3f
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

console.log('start');

var c = spawn(process.execPath, ['-e', 'while(true) { console.log("hi"); }']);

var sentKill = false;
var gotChildExit = true;

c.stdout.on('data', function(s) {
  // Prevent race condition:
  // Wait for the first bit of output from the child process
  // so that we're sure that it's in the V8 event loop and not
  // just in the startup phase of execution.
  if (!sentKill) {
    c.kill('SIGINT');
    console.log('SIGINT infinite-loop.js');
    sentKill = true;
  }
});

c.on('exit', function(code) {
  assert.ok(code !== 0);
  console.log('killed infinite-loop.js');
  gotChildExit = true;
});

process.on('exit', function() {
  assert.ok(sentKill);
  assert.ok(gotChildExit);
});


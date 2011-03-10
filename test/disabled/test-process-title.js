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

if (process.title === '') {
  console.log('skipping test -- not implemented for the host platform');
  //return;
}

// disabled because of two things
// - not tested on linux (can ps show process title on Linux?)
// - unable to verify effect on Darwin/OS X (only avail through GUI tools AFAIK)

function verifyProcessName(str, callback) {
  process.title = str;
  var buf = '';
  ps = spawn('ps');
  ps.stdout.setEncoding('utf8');
  ps.stdout.addListener('data', function(s) { buf += s; });
  ps.addListener('exit', function(c) {
    try {
      assert.equal(0, c);
      assert.ok(new RegExp(process.pid + ' ', 'm').test(buf));
      assert.ok(new RegExp(str, 'm').test(buf));
      callback();
    } catch (err) {
      callback(err);
    }
  });
}

verifyProcessName('3kd023mslkfp--unique-string--sksdf', function(err) {
  if (err) throw err;
  console.log('title is now %j', process.title);
  verifyProcessName('3kd023mslxxx--unique-string--xxx', function(err) {
    if (err) throw err;
    console.log('title is now %j', process.title);
  });
});

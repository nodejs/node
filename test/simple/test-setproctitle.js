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

// Original test written by Jakub Lekstan <kuebzky@gmail.com>

// FIXME add sunos support
if ('linux freebsd darwin'.indexOf(process.platform) === -1) {
  console.error('Skipping test, platform not supported.');
  process.exit();
}

var common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;

var title = 'testTestTESTtest123123123123123123HiHaiJo';

assert.notEqual(process.title, title);
process.title = title;
assert.equal(process.title, title);

exec('ps -p ' + process.pid + ' -o args=', function(error, stdout, stderr) {
  assert.equal(error, null);
  assert.equal(stderr, '');

  // freebsd always add ' (procname)' to the process title
  if (process.platform === 'freebsd') title += ' (node)';

  // omitting trailing whitespace and \n
  assert.equal(stdout.replace(/\s+$/, ''), title);
});

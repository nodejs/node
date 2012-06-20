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

// simulate `cat readfile.js | node readfile.js`

// TODO: Have some way to make this work on windows.
if (process.platform === 'win32') {
  console.error('No /dev/stdin on windows.  Skipping test.');
  process.exit();
}

var fs = require('fs');

var dataExpected = fs.readFileSync(__filename, 'utf8');

if (process.argv[2] === 'child') {
  fs.readFile('/dev/stdin', function(er, data) {
    if (er) throw er;
    process.stdout.write(data);
  });
  return;
}

var exec = require('child_process').exec;
var f = JSON.stringify(__filename);
var node = JSON.stringify(process.execPath);
var cmd = 'cat ' + f + ' | ' + node + ' ' + f + ' child';
exec(cmd, function(err, stdout, stderr) {
  if (err) console.error(err);
  assert(!err, 'it exits normally');
  assert(stdout === dataExpected, 'it reads the file and outputs it');
  assert(stderr === '', 'it does not write to stderr');
  console.log('ok');
});

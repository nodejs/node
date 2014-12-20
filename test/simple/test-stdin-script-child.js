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
var util = require('util');

var spawn = require('child_process').spawn;
var child = spawn(process.execPath, [], {
  env: util._extend(process.env, {
    NODE_DEBUG: process.argv[2]
  })
});
var wanted = child.pid + '\n';
var found = '';

child.stdout.setEncoding('utf8');
child.stdout.on('data', function(c) {
  found += c;
});

child.stderr.setEncoding('utf8');
child.stderr.on('data', function(c) {
  console.error('> ' + c.trim().split(/\n/).join('\n> '));
});

child.on('close', function(c) {
  assert(!c);
  assert.equal(found, wanted);
  console.log('ok');
});

setTimeout(function() {
  child.stdin.end('console.log(process.pid)');
});

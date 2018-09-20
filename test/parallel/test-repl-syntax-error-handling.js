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

'use strict';
require('../common');
const assert = require('assert');

switch (process.argv[2]) {
  case 'child':
    return child();
  case undefined:
    return parent();
  default:
    throw new Error('invalid');
}

function parent() {
  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child']);

  child.stderr.setEncoding('utf8');
  child.stderr.on('data', function(c) {
    console.error(`${c}`);
    throw new Error('should not get stderr data');
  });

  child.stdout.setEncoding('utf8');
  let out = '';
  child.stdout.on('data', function(c) {
    out += c;
  });
  child.stdout.on('end', function() {
    assert.strictEqual(out, '10\n');
    console.log('ok - got expected output');
  });

  child.on('exit', function(c) {
    assert(!c);
    console.log('ok - exit success');
  });
}

function child() {
  const vm = require('vm');
  let caught;
  try {
    vm.runInThisContext('haf!@##&$!@$*!@', { displayErrors: false });
  } catch (er) {
    caught = true;
  }
  assert(caught);
  vm.runInThisContext('console.log(10)', { displayErrors: false });
}

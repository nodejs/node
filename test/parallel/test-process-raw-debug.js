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
const common = require('../common');
const assert = require('assert');
const os = require('os');

switch (process.argv[2]) {
  case 'child':
    return child();
  case undefined:
    return parent();
  default:
    throw new Error(`wtf? ${process.argv[2]}`);
}

function parent() {
  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child']);

  let output = '';

  child.stderr.on('data', function(c) {
    output += c;
  });

  child.stderr.setEncoding('utf8');

  child.stderr.on('end', function() {
    assert.strictEqual(output, `I can still debug!${os.EOL}`);
    console.log('ok - got expected message');
  });

  child.on('exit', function(c) {
    assert(!c);
    console.log('ok - child exited nicely');
  });
}

function child() {
  // even when all hope is lost...

  process.nextTick = function() {
    throw new Error('No ticking!');
  };

  common.hijackStderr(common.mustNotCall('stderr.write must not be called.'));

  process._rawDebug('I can still %s!', 'debug');
}

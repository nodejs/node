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
const {
  mustCall,
  mustCallAtLeast,
} = require('../common');
const assert = require('assert');
const debug = require('util').debuglog('test');

let bufsize = 0;

switch (process.argv[2]) {
  case undefined:
    return parent();
  case 'child':
    return child();
  default:
    throw new Error('invalid');
}

function parent() {
  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child']);
  let sent = 0;

  let n = '';
  child.stdout.setEncoding('ascii');
  child.stdout.on('data', mustCallAtLeast((c) => {
    n += c;
  }));
  child.stdout.on('end', mustCall(() => {
    assert.strictEqual(+n, sent);
    debug('ok');
  }));

  // Write until the buffer fills up.
  let buf;
  do {
    bufsize += 1024;
    buf = Buffer.alloc(bufsize, '.');
    sent += bufsize;
  } while (child.stdin.write(buf));

  // Then write a bunch more times.
  for (let i = 0; i < 100; i++) {
    const buf = Buffer.alloc(bufsize, '.');
    sent += bufsize;
    child.stdin.write(buf);
  }

  // Now end, before it's all flushed.
  child.stdin.end();

  // now we wait...
}

function child() {
  let received = 0;
  process.stdin.on('data', mustCallAtLeast((c) => {
    received += c.length;
  }));
  process.stdin.on('end', mustCall(() => {
    // This console.log is part of the test.
    console.log(received);
  }));
}

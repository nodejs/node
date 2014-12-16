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
var BUFSIZE = 1024;

switch (process.argv[2]) {
  case undefined:
    return parent();
  case 'child':
    return child();
  default:
    throw new Error('wtf?');
}

function parent() {
  var spawn = require('child_process').spawn;
  var child = spawn(process.execPath, [__filename, 'child']);
  var sent = 0;

  var n = '';
  child.stdout.setEncoding('ascii');
  child.stdout.on('data', function(c) {
    n += c;
  });
  child.stdout.on('end', function() {
    assert.equal(+n, sent);
    console.log('ok');
  });

  // Write until the buffer fills up.
  do {
    var buf = new Buffer(BUFSIZE);
    buf.fill('.');
    sent += BUFSIZE;
  } while (child.stdin.write(buf));

  // then write a bunch more times.
  for (var i = 0; i < 100; i++) {
    var buf = new Buffer(BUFSIZE);
    buf.fill('.');
    sent += BUFSIZE;
    child.stdin.write(buf);
  }

  // now end, before it's all flushed.
  child.stdin.end();

  // now we wait...
}

function child() {
  var received = 0;
  process.stdin.on('data', function(c) {
    received += c.length;
  });
  process.stdin.on('end', function() {
    console.log(received);
  });
}

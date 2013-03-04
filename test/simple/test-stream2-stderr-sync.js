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

// Make sure that sync writes to stderr get processed before exiting.

var common = require('../common.js');
var assert = require('assert');

function parent() {
  var spawn = require('child_process').spawn;
  var assert = require('assert');
  var i = 0;
  children.forEach(function(_, c) {
    var child = spawn(process.execPath, [__filename, '' + c]);
    var err = '';

    child.stderr.on('data', function(c) {
      err += c;
    });

    child.on('close', function() {
      assert.equal(err, 'child ' + c + '\nfoo\nbar\nbaz\n');
      console.log('ok %d child #%d', ++i, c);
      if (i === children.length)
        console.log('1..' + i);
    });
  });
}

function child0() {
  // Just a very simple wrapper around TTY(2)
  // Essentially the same as stderr, but without all the net stuff.
  var Writable = require('stream').Writable;
  var util = require('util');

  // a lowlevel stderr writer
  var TTY = process.binding('tty_wrap').TTY;
  var handle = new TTY(2, false);

  util.inherits(W, Writable);

  function W(opts) {
    Writable.call(this, opts);
  }

  W.prototype._write = function(chunk, encoding, cb) {
    var req = handle.writeUtf8String(chunk.toString() + '\n');
    // here's the problem.
    // it needs to tell the Writable machinery that it's ok to write
    // more, but that the current buffer length is handle.writeQueueSize
    req.oncomplete = afterWrite
    if (req.writeQueueSize === 0)
      req.cb = cb;
    else
      cb();
  }
  function afterWrite(status, handle, req) {
    if (req.cb)
      req.cb();
  }

  var w = new W
  w.write('child 0');
  w.write('foo');
  w.write('bar');
  w.write('baz');
}

// using console.error
function child1() {
  console.error('child 1');
  console.error('foo');
  console.error('bar');
  console.error('baz');
}

// using process.stderr
function child2() {
  process.stderr.write('child 2\n');
  process.stderr.write('foo\n');
  process.stderr.write('bar\n');
  process.stderr.write('baz\n');
}

// using a net socket
function child3() {
  var net = require('net');
  var socket = new net.Socket({ fd: 2 });
  socket.write('child 3\n');
  socket.write('foo\n');
  socket.write('bar\n');
  socket.write('baz\n');
}


function child4() {
  console.error('child 4\nfoo\nbar\nbaz');
}

function child5() {
  process.stderr.write('child 5\nfoo\nbar\nbaz\n');
}

var children = [ child0, child1, child2, child3, child4, child5 ];

if (!process.argv[2]) {
  parent();
} else {
  children[process.argv[2]]();
  // immediate process.exit to kill any waiting stuff.
  process.exit();
}

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

// verify that stdout is never read from.
var net = require('net');
var read = net.Socket.prototype.read;

net.Socket.prototype.read = function() {
  if (this.fd === 1)
    throw new Error('reading from stdout!');
  if (this.fd === 2)
    throw new Error('reading from stderr!');
  return read.apply(this, arguments);
};

if (process.argv[2] === 'child')
  child();
else
  parent();

function parent() {
  var spawn = require('child_process').spawn;
  var node = process.execPath;
  var closes = 0;

  var c1 = spawn(node, [__filename, 'child']);
  var c1out = '';
  c1.stdout.setEncoding('utf8');
  c1.stdout.on('data', function(chunk) {
    c1out += chunk;
  });
  c1.stderr.setEncoding('utf8');
  c1.stderr.on('data', function(chunk) {
    console.error('c1err: ' + chunk.split('\n').join('\nc1err: '));
  });
  c1.on('close', function(code, signal) {
    closes++;
    assert(!code);
    assert(!signal);
    assert.equal(c1out, 'ok\n');
    console.log('ok');
  });

  var c2 = spawn(node, ['-e', 'console.log("ok")']);
  var c2out = '';
  c2.stdout.setEncoding('utf8');
  c2.stdout.on('data', function(chunk) {
    c2out += chunk;
  });
  c1.stderr.setEncoding('utf8');
  c1.stderr.on('data', function(chunk) {
    console.error('c1err: ' + chunk.split('\n').join('\nc1err: '));
  });
  c2.on('close', function(code, signal) {
    closes++;
    assert(!code);
    assert(!signal);
    assert.equal(c2out, 'ok\n');
    console.log('ok');
  });

  process.on('exit', function() {
    assert.equal(closes, 2, 'saw both closes');
  });
}

function child() {
  // should not be reading *ever* in here.
  net.Socket.prototype.read = function() {
    throw new Error('no reading allowed in child');
  };
  console.log('ok');
}

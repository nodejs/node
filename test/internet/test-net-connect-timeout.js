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

// This example attempts to time out before the connection is established
// https://groups.google.com/forum/#!topic/nodejs/UE0ZbfLt6t8
// https://groups.google.com/forum/#!topic/nodejs-dev/jR7-5UDqXkw
//
// TODO: how to do this without relying on the responses of specific sites?

var common = require('../common');
var net = require('net');
var assert = require('assert');

var start = new Date();

var gotTimeout0 = false;
var gotTimeout1 = false;

var gotConnect0 = false;
var gotConnect1 = false;

var T = 100;


// With DNS

var socket0 = net.createConnection(9999, 'google.com');

socket0.setTimeout(T);

socket0.on('timeout', function() {
  console.error('timeout');
  gotTimeout0 = true;
  var now = new Date();
  assert.ok(now - start < T + 500);
  socket0.destroy();
});

socket0.on('connect', function() {
  console.error('connect');
  gotConnect0 = true;
  socket0.destroy();
});


// Without DNS

var socket1 = net.createConnection(9999, '24.24.24.24');

socket1.setTimeout(T);

socket1.on('timeout', function() {
  console.error('timeout');
  gotTimeout1 = true;
  var now = new Date();
  assert.ok(now - start < T + 500);
  socket1.destroy();
});

socket1.on('connect', function() {
  console.error('connect');
  gotConnect1 = true;
  socket1.destroy();
});





process.on('exit', function() {
  assert.ok(gotTimeout0);
  assert.ok(!gotConnect0);

  assert.ok(gotTimeout1);
  assert.ok(!gotConnect1);
});

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

// This example sets a timeout then immediately attempts to disable the timeout
// https://github.com/joyent/node/pull/2245

var common = require('../common');
var net = require('net');
var assert = require('assert');

var T = 100;

var server = net.createServer(function(c) {
  c.write('hello');
});
server.listen(common.PORT);

var killers = [0, Infinity, NaN];

var left = killers.length;
killers.forEach(function(killer) {
  var socket = net.createConnection(common.PORT, 'localhost');

  socket.setTimeout(T, function() {
    socket.destroy();
    if (--left === 0) server.close();
    assert.ok(killer !== 0);
    clearTimeout(timeout);
  });

  socket.setTimeout(killer);

  var timeout = setTimeout(function() {
    socket.destroy();
    if (--left === 0) server.close();
    assert.ok(killer === 0);
  }, T * 2);
});

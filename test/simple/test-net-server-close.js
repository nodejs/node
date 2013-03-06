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
var net = require('net');

var events = [];
var sockets = [];

process.on('exit', function() {
  assert.equal(server.connections, 0);
  assert.equal(events.length, 3);
  // Expect to see one server event and two client events. The order of the
  // events is undefined because they arrive on the same event loop tick.
  assert.equal(events.join(' ').match(/server/g).length, 1);
  assert.equal(events.join(' ').match(/client/g).length, 2);
});

var server = net.createServer(function(c) {
  c.on('close', function() {
    events.push('client');
  });

  sockets.push(c);

  if (sockets.length === 2) {
    server.close();
    sockets.forEach(function(c) { c.destroy() });
  }
});

server.on('close', function() {
  events.push('server');
});

server.listen(common.PORT, function() {
  net.createConnection(common.PORT);
  net.createConnection(common.PORT);
});

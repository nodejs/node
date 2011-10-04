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

// This tests binds to one port, then attempts to start a server on that
// port. It should be EADDRINUSE but be able to then bind to another port.
var common = require('../common');
var assert = require('assert');
var net = require('net');

var connections = 0;

var server1listening = false;
var server2listening = false;

var server1 = net.Server(function(socket) {
  connections++;
  socket.destroy();
});

var server2 = net.Server(function(socket) {
  connections++;
  socket.destroy();
});

var server2errors = 0;
server2.on('error', function() {
  server2errors++;
  console.error('server2 error');
});


server1.listen(common.PORT, function() {
  console.error('server1 listening');
  server1listening = true;
  // This should make server2 emit EADDRINUSE
  server2.listen(common.PORT);

  // Wait a bit, now try again.
  // TODO, the listen callback should report if there was an error.
  // Then we could avoid this very unlikely but potential race condition
  // here.
  setTimeout(function() {
    server2.listen(common.PORT + 1, function() {
      console.error('server2 listening');
      server2listening = true;

      server1.close();
      server2.close();
    });
  }, 100);
});


process.on('exit', function() {
  assert.equal(1, server2errors);
  assert.ok(server2listening);
  assert.ok(server1listening);
});

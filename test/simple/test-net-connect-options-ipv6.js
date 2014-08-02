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
var dns = require('dns');

if (!common.hasIPv6) {
  console.error('Skipping test, no IPv6 support');
  return;
}

var serverGotEnd = false;
var clientGotEnd = false;

dns.lookup('localhost', 6, function(err) {
  if (err) {
    console.error('Looks like IPv6 is not really supported');
    console.error(err);
    return;
  }

  var server = net.createServer({allowHalfOpen: true}, function(socket) {
    socket.resume();
    socket.on('end', function() {
      serverGotEnd = true;
    });
    socket.end();
  });

  server.listen(common.PORT, '::1', function() {
    var client = net.connect({
      host: 'localhost',
      port: common.PORT,
      family: 6,
      allowHalfOpen: true
    }, function() {
      console.error('client connect cb');
      client.resume();
      client.on('end', function() {
        clientGotEnd = true;
        setTimeout(function() {
          assert(client.writable);
          client.end();
        }, 10);
      });
      client.on('close', function() {
        server.close();
      });
    });
  });

  process.on('exit', function() {
    console.error('exit', serverGotEnd, clientGotEnd);
    assert(serverGotEnd);
    assert(clientGotEnd);
  });
});

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
if (!common.hasIPv6)
  common.skip('no IPv6 support');

const assert = require('assert');
const net = require('net');

const hosts = common.localIPv6Hosts;
let hostIdx = 0;
let host = hosts[hostIdx];
let localhostTries = 10;

const server = net.createServer({allowHalfOpen: true}, function(socket) {
  socket.resume();
  socket.on('end', common.mustCall());
  socket.end();
});

server.listen(0, '::1', tryConnect);

function tryConnect() {
  const client = net.connect({
    host: host,
    port: server.address().port,
    family: 6,
    allowHalfOpen: true
  }, function() {
    console.error('client connect cb');
    client.resume();
    client.on('end', common.mustCall(function() {
      setTimeout(function() {
        assert(client.writable);
        client.end();
      }, 10);
    }));
    client.on('close', function() {
      server.close();
    });
  }).on('error', function(err) {
    // ENOTFOUND means we don't have the requested address. In this
    // case we try the next one in the list and if we run out of
    // candidates we assume IPv6 is not supported on the
    // machine and skip the test.
    // EAI_AGAIN means we tried to remotely resolve the address and
    // timed out or hit some intermittent connectivity issue with the
    // dns server.  Although we are looking for local loopback addresses
    // we may go remote since the list we search includes addresses that
    // cover more than is available on any one distribution. The
    // net is that if we get an EAI_AGAIN we were looking for an
    // address which does not exist in this distribution so the error
    // is not significant and we should just move on and try the
    // next address in the list.
    if ((err.syscall === 'getaddrinfo') && ((err.code === 'ENOTFOUND') ||
                                            (err.code === 'EAI_AGAIN'))) {
      if (host !== 'localhost' || --localhostTries === 0)
        host = hosts[++hostIdx];
      if (host)
        tryConnect();
      else {
        server.close();
        common.skip('no IPv6 localhost support');
      }
      return;
    }
    throw err;
  });
}

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
// Verify that the 'upgrade' header causes an 'upgrade' event to be emitted to
// the HTTP client. This test uses a raw TCP server to better control server
// behavior.

const common = require('../common');
const assert = require('assert');

const http = require('http');
const net = require('net');
const Countdown = require('../common/countdown');

// Create a TCP server
const srv = net.createServer(function(c) {
  c.on('data', function(d) {
    c.write('HTTP/1.1 101\r\n');
    c.write('hello: world\r\n');
    c.write('connection: upgrade\r\n');
    c.write('upgrade: websocket\r\n');
    c.write('\r\n');
    c.write('nurtzo');
  });

  c.on('end', function() {
    c.end();
  });
});

srv.listen(0, '127.0.0.1', common.mustCall(function() {
  const port = this.address().port;
  const headers = [
    {
      connection: 'upgrade',
      upgrade: 'websocket'
    },
    [
      ['Host', 'echo.websocket.org'],
      ['Connection', 'Upgrade'],
      ['Upgrade', 'websocket'],
      ['Origin', 'http://www.websocket.org']
    ]
  ];
  const countdown = new Countdown(headers.length, () => srv.close());

  headers.forEach(function(h) {
    const req = http.get({
      port: port,
      headers: h
    });
    let sawUpgrade = false;
    req.on('upgrade', common.mustCall(function(res, socket, upgradeHead) {
      sawUpgrade = true;
      let recvData = upgradeHead;
      socket.on('data', function(d) {
        recvData += d;
      });

      socket.on('close', common.mustCall(function() {
        assert.strictEqual(recvData.toString(), 'nurtzo');
      }));

      console.log(res.headers);
      const expectedHeaders = {
        hello: 'world',
        connection: 'upgrade',
        upgrade: 'websocket'
      };
      assert.deepStrictEqual(expectedHeaders, res.headers);

      socket.end();
      countdown.dec();
    }));
    req.on('close', common.mustCall(function() {
      assert.strictEqual(sawUpgrade, true);
    }));
  });
}));

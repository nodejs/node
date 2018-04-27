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
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the data received through tls over http tunnel
// is same as what is sent.

const assert = require('assert');
const https = require('https');
const net = require('net');
const http = require('http');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

const options = { key, cert };

const server = https.createServer(options, common.mustCall((req, res) => {
  console.log('SERVER: got request');
  res.writeHead(200, {
    'content-type': 'text/plain'
  });
  console.log('SERVER: sending response');
  res.end('hello world\n');
}));

const proxy = net.createServer((clientSocket) => {
  console.log('PROXY: got a client connection');

  let serverSocket = null;

  clientSocket.on('data', (chunk) => {
    console.log('PROXY: write to SERVER');
    if (!serverSocket) {
      // Verify the CONNECT request
      assert.strictEqual(`CONNECT localhost:${server.address().port} ` +
                         'HTTP/1.1\r\n' +
                         'Proxy-Connections: keep-alive\r\n' +
                         `Host: localhost:${proxy.address().port}\r\n` +
                         'Connection: close\r\n\r\n',
                         chunk.toString());

      console.log('PROXY: got CONNECT request');
      console.log('PROXY: creating a tunnel');

      // create the tunnel
      serverSocket = net.connect(server.address().port, common.mustCall(() => {
        console.log('PROXY: replying to client CONNECT request');

        // Send the response
        clientSocket.write('HTTP/1.1 200 OK\r\nProxy-Connections: keep' +
          '-alive\r\nConnection: keep-alive\r\nVia: ' +
          `localhost:${proxy.address().port}\r\n\r\n`);
      }));

      serverSocket.on('data', (chunk) => {
        console.log('PROXY: write to CLIENT');
        if (clientSocket.writable)
          clientSocket.write(chunk);
      });

      serverSocket.on('end', () => {
        console.log('PROXY: server readable end');
        clientSocket.destroy();
      });
      serverSocket.on('close', common.mustCall(() => {
        console.log('PROXY: got internal CLOSE event');
        maybeFinish();
      }));
    } else if (serverSocket.writable) {
      serverSocket.write(chunk);
    }
  });

  clientSocket.on('end', () => {
    console.log('PROXY: client readable end');
    serverSocket.destroy();
  });
  clientSocket.on('close', common.mustCall(() => {
    console.log('PROXY: got external CLOSE event');
    maybeFinish();
  }));

  function maybeFinish() {
    if (!serverSocket.writable && !clientSocket.writable) {
      proxy.close();
      server.close();
    }
  }
});
server.listen(0);

proxy.listen(0, common.mustCall(() => {
  console.log('CLIENT: Making CONNECT request');

  const req = http.request({
    port: proxy.address().port,
    method: 'CONNECT',
    path: `localhost:${server.address().port}`,
    headers: {
      'Proxy-Connections': 'keep-alive'
    }
  });
  req.on('connect', onConnect);   // for v0.7 or later
  req.end();

  function onConnect(res, socket, header) {
    assert.strictEqual(200, res.statusCode);
    console.log('CLIENT: got CONNECT response');
    console.log('CLIENT: Making HTTPS request');

    https.get({
      path: '/foo',
      key: key,
      cert: cert,
      socket: socket,  // reuse the socket
      agent: false,
      rejectUnauthorized: false
    }, common.mustCall((res) => {
      assert.strictEqual(200, res.statusCode);

      res.on('data', common.mustCall((chunk) => {
        assert.strictEqual('hello world\n', chunk.toString());
        console.log('CLIENT: got HTTPS response');
      }));

      res.on('end', common.mustCall());
    }));
  }
}));

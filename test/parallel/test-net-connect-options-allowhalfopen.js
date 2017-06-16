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
const assert = require('assert');
const net = require('net');

function testClients(getSocketOpt, getConnectOpt, getConnectCb) {
  const cloneOptions = (index) =>
    Object.assign({}, getSocketOpt(index), getConnectOpt(index));
  return [
    net.connect(cloneOptions(0), getConnectCb(0)),
    net.connect(cloneOptions(1))
      .on('connect', getConnectCb(1)),
    net.createConnection(cloneOptions(2), getConnectCb(2)),
    net.createConnection(cloneOptions(3))
      .on('connect', getConnectCb(3)),
    new net.Socket(getSocketOpt(4)).connect(getConnectOpt(4), getConnectCb(4)),
    new net.Socket(getSocketOpt(5)).connect(getConnectOpt(5))
      .on('connect', getConnectCb(5))
  ];
}

const CLIENT_VARIANTS = 6;  // Same length as array above
const forAllClients = (cb) => common.mustCall(cb, CLIENT_VARIANTS);

// Test allowHalfOpen
{
  let clientReceivedFIN = 0;
  let serverConnections = 0;
  let clientSentFIN = 0;
  let serverReceivedFIN = 0;
  const server = net.createServer({
    allowHalfOpen: true
  })
  .on('connection', forAllClients(function serverOnConnection(socket) {
    const serverConnection = ++serverConnections;
    let clientId;
    console.error(`${serverConnections} 'connection' emitted on server`);
    socket.resume();
    // 'end' on each socket must not be emitted twice
    socket.on('data', common.mustCall(function(data) {
      clientId = data.toString();
      console.error(`${serverConnection} server connection is started ` +
                    `by client No. ${clientId}`);
    }));
    socket.on('end', common.mustCall(function() {
      serverReceivedFIN++;
      console.error(`Server received FIN sent by No. ${clientId}`);
      if (serverReceivedFIN === CLIENT_VARIANTS) {
        setTimeout(() => {
          server.close();
          console.error(`No. ${clientId} connection is closing server: ` +
                        `${serverReceivedFIN} FIN received by server, ` +
                        `${clientReceivedFIN} FIN received by client, ` +
                        `${clientSentFIN} FIN sent by client, ` +
                        `${serverConnections} FIN sent by server`);
        }, 50);
      }
    }, 1));
    socket.end();
    console.error(`Server has sent ${serverConnections} FIN`);
  }))
  .on('close', common.mustCall(function serverOnClose() {
    console.error('Server has been closed: ' +
                  `${serverReceivedFIN} FIN received by server, ` +
                  `${clientReceivedFIN} FIN received by client, ` +
                  `${clientSentFIN} FIN sent by client, ` +
                  `${serverConnections} FIN sent by server`);
  }))
  .listen(0, 'localhost', common.mustCall(function serverOnListen() {
    const host = 'localhost';
    const port = server.address().port;

    console.error(`Server starts at ${host}:${port}`);
    const getSocketOpt = () => ({ allowHalfOpen: true });
    const getConnectOpt = () => ({ host, port });
    const getConnectCb = (index) => common.mustCall(function clientOnConnect() {
      const client = this;
      console.error(`'connect' emitted on Client ${index}`);
      client.resume();
      client.on('end', common.mustCall(function clientOnEnd() {
        setTimeout(function() {
          // when allowHalfOpen is true, client must still be writable
          // after the server closes the connections, but not readable
          console.error(`No. ${index} client received FIN`);
          assert(!client.readable);
          assert(client.writable);
          assert(client.write(String(index)));
          client.end();
          clientSentFIN++;
          console.error(`No. ${index} client sent FIN, ` +
                        `${clientSentFIN} have been sent`);
        }, 50);
      }));
      client.on('close', common.mustCall(function clientOnClose() {
        clientReceivedFIN++;
        console.error(`No. ${index} connection has been closed by both ` +
                      `sides, ${clientReceivedFIN} clients have closed`);
      }));
    });

    testClients(getSocketOpt, getConnectOpt, getConnectCb);
  }));
}

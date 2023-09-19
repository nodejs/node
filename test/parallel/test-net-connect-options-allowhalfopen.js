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

// Test allowHalfOpen
{
  let clientReceivedFIN = 0;
  let serverConnections = 0;
  let clientSentFIN = 0;
  let serverReceivedFIN = 0;
  const host = common.localhostIPv4;

  function serverOnConnection(socket) {
    console.log(`'connection' ${++serverConnections} emitted on server`);
    const srvConn = serverConnections;
    socket.resume();
    socket.on('data', common.mustCall(function socketOnData(data) {
      this.clientId = data.toString();
      console.log(
        `server connection ${srvConn} is started by client ${this.clientId}`);
    }));
    // 'end' on each socket must not be emitted twice
    socket.on('end', common.mustCall(function socketOnEnd() {
      console.log(`Server received FIN sent by client ${this.clientId}`);
      if (++serverReceivedFIN < CLIENT_VARIANTS) return;
      setTimeout(() => {
        server.close();
        console.log(`connection ${this.clientId} is closing the server:
          FIN ${serverReceivedFIN} received by server,
          FIN ${clientReceivedFIN} received by client
          FIN ${clientSentFIN} sent by client,
          FIN ${serverConnections} sent by server`.replace(/ {3,}/g, ''));
      }, 50);
    }, 1));
    socket.end();
    console.log(`Server has sent ${serverConnections} FIN`);
  }

  // These two levels of functions (and not arrows) are necessary in order to
  // bind the `index`, and the calling socket (`this`)
  function clientOnConnect(index) {
    return common.mustCall(function clientOnConnectInner() {
      const client = this;
      console.log(`'connect' emitted on Client ${index}`);
      client.resume();
      client.on('end', common.mustCall(function clientOnEnd() {
        setTimeout(function closeServer() {
          // When allowHalfOpen is true, client must still be writable
          // after the server closes the connections, but not readable
          console.log(`client ${index} received FIN`);
          assert(!client.readable);
          assert(client.writable);
          assert(client.write(String(index)));
          client.end();
          clientSentFIN++;
          console.log(
            `client ${index} sent FIN, ${clientSentFIN} have been sent`);
        }, 50);
      }));
      client.on('close', common.mustCall(function clientOnClose() {
        clientReceivedFIN++;
        console.log(`connection ${index} has been closed by both sides,` +
          ` ${clientReceivedFIN} clients have closed`);
      }));
    });
  }

  function serverOnClose() {
    console.log(`Server has been closed:
      FIN ${serverReceivedFIN} received by server
      FIN ${clientReceivedFIN} received by client
      FIN ${clientSentFIN} sent by client
      FIN ${serverConnections} sent by server`.replace(/ {3,}/g, ''));
  }

  function serverOnListen() {
    const port = server.address().port;
    console.log(`Server started listening at ${host}:${port}`);
    const opts = { allowHalfOpen: true, host, port };
    // 6 variations === CLIENT_VARIANTS
    net.connect(opts, clientOnConnect(1));
    net.connect(opts).on('connect', clientOnConnect(2));
    net.createConnection(opts, clientOnConnect(3));
    net.createConnection(opts).on('connect', clientOnConnect(4));
    new net.Socket(opts).connect(opts, clientOnConnect(5));
    new net.Socket(opts).connect(opts).on('connect', clientOnConnect(6));
  }

  const CLIENT_VARIANTS = 6;

  // The trigger
  const server = net.createServer({ allowHalfOpen: true })
    .on('connection', common.mustCall(serverOnConnection, CLIENT_VARIANTS))
    .on('close', common.mustCall(serverOnClose))
    .listen(0, host, common.mustCall(serverOnListen));
}

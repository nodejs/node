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

const assert = require('assert');
const tls = require('tls');
const net = require('net');
const fs = require('fs');
const path = require('path');

const options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

const server = tls.createServer(options, common.mustCall((socket) => {
  socket.end('Hello');
}, 2)).listen(0, common.mustCall(() => {
  let waiting = 2;
  function establish(socket, calls) {
    const client = tls.connect({
      rejectUnauthorized: false,
      socket: socket
    }, common.mustCall(() => {
      let data = '';
      client.on('data', common.mustCall((chunk) => {
        data += chunk.toString();
      }));
      client.on('end', common.mustCall(() => {
        assert.strictEqual(data, 'Hello');
        if (--waiting === 0)
          server.close();
      }));
    }, calls));
    assert(client.readable);
    assert(client.writable);

    return client;
  }

  const { port } = server.address();

  // Immediate death socket
  const immediateDeath = net.connect(port);
  establish(immediateDeath, 0).destroy();

  // Outliving
  const outlivingTCP = net.connect(port, common.mustCall(() => {
    outlivingTLS.destroy();
    next();
  }));
  const outlivingTLS = establish(outlivingTCP, 0);

  function next() {
    // Already connected socket
    const connected = net.connect(port, common.mustCall(() => {
      establish(connected);
    }));

    // Connecting socket
    const connecting = net.connect(port);
    establish(connecting);
  }
}));

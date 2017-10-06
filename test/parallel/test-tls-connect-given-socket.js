'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const tls = require('tls');
const net = require('net');

const options = {
  key: fixtures.readSync('test_key.pem'),
  cert: fixtures.readSync('test_cert.pem')
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

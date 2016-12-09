'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const net = require('net');
const fs = require('fs');
const path = require('path');

let serverConnected = 0;
let clientConnected = 0;

const options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

const server = tls.createServer(options, (socket) => {
  serverConnected++;
  socket.end('Hello');
}).listen(0, () => {
  let waiting = 2;
  function establish(socket) {
    const client = tls.connect({
      rejectUnauthorized: false,
      socket: socket
    }, () => {
      clientConnected++;
      let data = '';
      client.on('data', common.mustCall((chunk) => {
        data += chunk.toString();
      }));
      client.on('end', common.mustCall(() => {
        assert.strictEqual(data, 'Hello');
        if (--waiting === 0)
          server.close();
      }));
    });
    assert(client.readable);
    assert(client.writable);

    return client;
  }

  const { port } = server.address();

  // Immediate death socket
  const immediateDeath = net.connect(port);
  establish(immediateDeath).destroy();

  // Outliving
  const outlivingTCP = net.connect(port, common.mustCall(() => {
    outlivingTLS.destroy();
    next();
  }));
  const outlivingTLS = establish(outlivingTCP);

  function next() {
    // Already connected socket
    const connected = net.connect(port, common.mustCall(() => {
      establish(connected);
    }));

    // Connecting socket
    const connecting = net.connect(port);
    establish(connecting);
  }
});

process.on('exit', () => {
  assert.strictEqual(serverConnected, 2);
  assert.strictEqual(clientConnected, 2);
});

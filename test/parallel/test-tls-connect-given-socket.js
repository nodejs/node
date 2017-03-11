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

const server = tls.createServer(options, function(socket) {
  serverConnected++;
  socket.end('Hello');
}).listen(0, function() {
  let waiting = 2;
  function establish(socket) {
    const client = tls.connect({
      rejectUnauthorized: false,
      socket: socket
    }, function() {
      clientConnected++;
      let data = '';
      client.on('data', function(chunk) {
        data += chunk.toString();
      });
      client.on('end', function() {
        assert.equal(data, 'Hello');
        if (--waiting === 0)
          server.close();
      });
    });
    assert(client.readable);
    assert(client.writable);

    return client;
  }

  // Immediate death socket
  const immediateDeath = net.connect(this.address().port);
  establish(immediateDeath).destroy();

  // Outliving
  const outlivingTCP = net.connect(this.address().port);
  outlivingTCP.on('connect', function() {
    outlivingTLS.destroy();
    next();
  });
  const outlivingTLS = establish(outlivingTCP);

  function next() {
    // Already connected socket
    const connected = net.connect(server.address().port, function() {
      establish(connected);
    });

    // Connecting socket
    const connecting = net.connect(server.address().port);
    establish(connecting);

  }
});

process.on('exit', function() {
  assert.equal(serverConnected, 2);
  assert.equal(clientConnected, 2);
});

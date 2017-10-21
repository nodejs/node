'use strict';
require('../common');
const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readSync('test_key.pem'),
  cert: fixtures.readSync('test_cert.pem')
};

const iter = 10;

const server = tls.createServer(options, (socket) => {
  socket.pipe(socket);
  socket.on('end', () => {
    server.close();
  });
});

server.listen(0, () => {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false
  }, () => {
    for (let i = 1; i < iter; i++) {
      client.write('a');
      assert.strictEqual(client.bufferSize, i);
    }
    client.end();
  });

  client.on('finish', () => {
    assert.strictEqual(client.bufferSize, 0);
  });
});

'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer().listen(common.PORT, connectToServer);

function connectToServer() {
  const client = net.createConnection(common.PORT, () => {
    assert.throws(() => {
      client.write(1337);
    }, /Invalid data, chunk must be a string or buffer, not number/);

    client.end();
  })
  .on('end', () => server.close());
}

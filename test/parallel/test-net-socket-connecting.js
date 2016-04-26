'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer((conn) => {
  conn.end();
  server.close();
}).listen(common.PORT, () => {
  const client = net.connect(common.PORT, () => {
    assert.equal(client.connecting, false);

    // Legacy getter
    assert.equal(client._connecting, false);
    client.end();
  })
  assert.equal(client.connecting, true);

  // Legacy getter
  assert.equal(client._connecting, true);
});

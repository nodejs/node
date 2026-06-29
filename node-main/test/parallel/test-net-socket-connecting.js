'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer((conn) => {
  conn.end();
  server.close();
}).listen(0, () => {
  const client = net.connect(server.address().port, () => {
    assert.strictEqual(client.connecting, false);

    // Legacy getter
    assert.strictEqual(client._connecting, false);
    client.end();
  });
  assert.strictEqual(client.connecting, true);

  // Legacy getter
  assert.strictEqual(client._connecting, true);
});

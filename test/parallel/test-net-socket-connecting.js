'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer((conn) => {
  conn.end();
  server.close();
}).listen(common.PORT, () => {
  const client = net.connect(common.PORT, () => {
    assert.strictEqual(client.connecting, false);

    // Legacy getter
    assert.strictEqual(client._connecting, false);
    client.end();
  });
  assert.strictEqual(client.connecting, true);

  // Legacy getter
  assert.strictEqual(client._connecting, true);
});

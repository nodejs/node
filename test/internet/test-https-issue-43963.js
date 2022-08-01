'use strict';
const common = require('../common');
const https = require('node:https');
const assert = require('node:assert');

const server = https.createServer();

server.on(
  'tlsClientError',
  common.mustCall((exception, tlsSocket) => {
    assert.strictEqual(exception !== undefined, true);
    assert.strictEqual(Object.keys(tlsSocket.address()).length !== 0, true);
    assert.strictEqual(tlsSocket.localAddress !== undefined, true);
    assert.strictEqual(tlsSocket.localPort !== undefined, true);
    assert.strictEqual(tlsSocket.remoteAddress !== undefined, true);
    assert.strictEqual(tlsSocket.remoteFamily !== undefined, true);
    assert.strictEqual(tlsSocket.remotePort !== undefined, true);
  }),
);

server.listen(0, () => {
  const req = https.request({
    hostname: '127.0.0.1',
    port: server.address().port,
  });
  req.on(
    'error',
    common.mustCall(() => server.close()),
  );
  req.end();
});

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const server = tls.createServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
  // Amount of keylog events depends on negotiated protocol
  // version, so force a specific one:
  minVersion: 'TLSv1.3',
  maxVersion: 'TLSv1.3',
}).listen(() => {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
  });

  server.on('keylog', common.mustCall((line, tlsSocket) => {
    assert(Buffer.isBuffer(line));
    assert.strictEqual(tlsSocket.encrypted, true);
  }, 5));
  client.on('keylog', common.mustCall((line) => {
    assert(Buffer.isBuffer(line));
  }, 5));

  client.once('secureConnect', () => {
    server.close();
    client.end();
  });
});

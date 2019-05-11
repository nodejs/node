'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
if (/^(0\.|1\.0\.|1\.1\.0)/.test(process.versions.openssl))
  common.skip('keylog support not available');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const server = tls.createServer({
  key: fixtures.readSync('/keys/agent2-key.pem'),
  cert: fixtures.readSync('/keys/agent2-cert.pem'),
  // Amount of keylog events depends on negotiated protocol
  // version, so force a specific one:
  minVersion: 'TLSv1.2',
  maxVersion: 'TLSv1.2',
}).listen(() => {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
  });

  const verifyBuffer = (line) => assert(Buffer.isBuffer(line));
  server.on('keylog', common.mustCall(verifyBuffer, 1));
  client.on('keylog', common.mustCall(verifyBuffer, 1));

  client.once('secureConnect', () => {
    server.close();
    client.end();
  });
});

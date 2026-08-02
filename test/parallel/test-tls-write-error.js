'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { TestTLSSocket, ccs } = require('../common/tls');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const https = require('https');
const { hasFIPS } = require('../common/crypto');

// Regression test for an use-after-free bug in the TLS implementation that
// would occur when `SSL_write()` failed.
// Refs: https://github.com/nodejs-private/security/issues/189

const server_key = fixtures.readKey('agent1-key.pem');
const server_cert = fixtures.readKey('agent1-cert.pem');

const opts = {
  key: server_key,
  cert: server_cert,
};
const rejectsClientHello = hasFIPS(3) && !hasFIPS(3, 5);

if (!process.features.openssl_is_boringssl) {
  opts.ciphers = 'ALL@SECLEVEL=0';
}

const server = https.createServer(opts, (req, res) => {
  res.write('hello');
});

if (rejectsClientHello) {
  server.once('tlsClientError', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_SSL_WRONG_SIGNATURE_TYPE');
  }));
}

server.listen(0, common.mustCall(() => {
  const client = new TestTLSSocket(server_cert);

  client.connect({
    host: 'localhost',
    port: server.address().port
  }, common.mustCall(() => {
    const ch = client.createClientHello();
    client.write(ch);
  }));

  client.once('data', common.mustCall((buf) => {
    if (rejectsClientHello) {
      client.end();
      server.close();
      return;
    }

    let remaining = buf;
    do {
      remaining = client.parseTLSFrame(remaining);
    } while (remaining.length > 0);

    const cke = client.createClientKeyExchange();
    const finished = client.createFinished();
    const ill = client.createIllegalHandshake();
    const frames = Buffer.concat([
      cke,
      ccs,
      client.encrypt(finished),
      client.encrypt(ill),
    ]);
    client.write(frames, common.mustCall(() => {
      client.end();
      server.close();
    }));
  }));
}));

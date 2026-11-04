'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const tls = require('tls');
const fixtures = require('../common/fixtures');

for (const customConnection of [false, true]) {
  const server = http2.createSecureServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
    requestCert: true,
    rejectUnauthorized: true,
    minVersion: 'TLSv1.3',
    maxVersion: 'TLSv1.3',
  });
  server.on('tlsClientError', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_SSL_PEER_DID_NOT_RETURN_A_CERTIFICATE');
  }));
  server.listen(0, '127.0.0.1', common.mustCall(() => {
    const options = {
      host: '127.0.0.1',
      port: server.address().port,
      servername: 'localhost',
      rejectUnauthorized: false,
      ALPNProtocols: ['h2'],
      minVersion: 'TLSv1.3',
      maxVersion: 'TLSv1.3',
    };
    const socket = customConnection ? tls.connect(options) : undefined;
    const session = http2.connect(`https://127.0.0.1:${server.address().port}`, {
      ...options,
      ...(customConnection ? { createConnection: () => socket } : {}),
    });
    const transport = socket || session.socket;
    // Destroying the socket must not emit the TLS error a second time.
    transport.on('error', common.mustCall());
    let errorEmitted = false;
    session.on('connect', common.mustCall(() => {
      assert.strictEqual(transport.getProtocol(), 'TLSv1.3');
    }));
    session.on('error', common.mustCall((err) => {
      // OpenSSL and BoringSSL use different names for the same TLS alert.
      assert.ok([
        'ERR_SSL_TLSV13_ALERT_CERTIFICATE_REQUIRED',
        'ERR_SSL_TLSV1_ALERT_CERTIFICATE_REQUIRED',
      ].includes(err.code), `Unexpected TLS error code: ${err.code}`);
      errorEmitted = true;
    }));
    transport.on('close', common.mustCall(function() {
      // The session's socket proxy may already be detached.
      assert(this.destroyed);
    }));
    session.on('close', common.mustCall(() => {
      assert(errorEmitted);
      assert(session.destroyed);
      if (socket) assert(socket.destroyed);
      server.close(common.mustCall());
    }));
  }));
}

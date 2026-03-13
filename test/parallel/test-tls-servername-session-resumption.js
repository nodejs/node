'use strict';

// Verify that servername (SNI) is preserved on resumed TLS sessions.
// Regression test for https://github.com/nodejs/node/issues/59202

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// The fix relies on SSL_SESSION_get0_hostname which BoringSSL may not support.
if (process.features.openssl_is_boringssl)
  common.skip('BoringSSL does not support SSL_SESSION_get0_hostname');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const SERVERNAME = 'agent1.example.com';

function test(minVersion, maxVersion) {
  const serverOptions = {
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
    minVersion,
    maxVersion,
  };

  let serverConns = 0;
  const server = tls.createServer(serverOptions, common.mustCall((socket) => {
    assert.strictEqual(socket.servername, SERVERNAME);
    serverConns++;
    // Don't end the socket immediately on the first connection — the client
    // needs time to receive the TLS 1.3 NewSessionTicket message.
    if (serverConns === 2)
      socket.end();
  }, 2));

  server.listen(0, common.mustCall(function() {
    const port = server.address().port;

    // First connection: establish a session with an SNI servername.
    const client1 = tls.connect({
      port,
      servername: SERVERNAME,
      rejectUnauthorized: false,
      minVersion,
      maxVersion,
    }, common.mustCall());

    client1.once('session', common.mustCall((session) => {
      client1.end();

      // Second connection: resume the session and verify the servername is
      // preserved on the server side.
      const client2 = tls.connect({
        port,
        servername: SERVERNAME,
        rejectUnauthorized: false,
        session,
        minVersion,
        maxVersion,
      }, common.mustCall(() => {
        assert.strictEqual(client2.isSessionReused(), true);
        client2.end();
      }));

      client2.on('close', common.mustCall(() => server.close()));
    }));

    client1.resume();
  }));
}

// TLS 1.3 — the primary bug: SSL_get_servername() returns NULL on
// server-side resumed sessions.
test('TLSv1.3', 'TLSv1.3');

// TLS 1.2 — OpenSSL handles this natively, but verify the fallback path
// doesn't break it.
test('TLSv1.2', 'TLSv1.2');

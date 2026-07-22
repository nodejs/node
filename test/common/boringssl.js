/* eslint-disable node-core/crypto-check */

'use strict';
const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');

// This module is for BoringSSL-specific branches in tests whose original
// OpenSSL coverage cannot run unchanged. Each helper should assert the
// observable BoringSSL behavior that explains why the OpenSSL-specific
// assertions are bypassed.

/**
 * BoringSSL exposes many removed or disabled TLS cipher suites as "no match"
 * at secure-context creation time. This is used for suites such as
 * finite-field DHE and anonymous ECDH that OpenSSL builds may still negotiate
 * in tests.
 * @param {Function} fn
 */
function assertNoCipherMatch(fn) {
  assert.throws(fn, {
    code: 'ERR_SSL_NO_CIPHER_MATCH',
    library: 'SSL routines',
    function: 'OPENSSL_internal',
    reason: 'NO_CIPHER_MATCH',
  });
}

/**
 * BoringSSL does not parse OpenSSL cipher-string commands such as `@SECLEVEL`.
 * Those are OpenSSL policy directives, not cipher names.
 * @param {Function} fn
 */
function assertInvalidCommand(fn) {
  assert.throws(fn, {
    code: 'ERR_SSL_INVALID_COMMAND',
    library: 'SSL routines',
    function: 'OPENSSL_internal',
    reason: 'INVALID_COMMAND',
  });
}

/**
 * Node's DHE tests exercise OpenSSL's finite-field DHE cipher support and DH
 * parameter-size policy. BoringSSL does not offer these DHE cipher suites on
 * this surface, so creating a server context with a DHE-only cipher list fails
 * before a handshake can test DH parameter behavior.
 */
function assertFiniteFieldDheUnsupported() {
  assertNoCipherMatch(() => {
    tls.createServer({
      key: fixtures.readKey('agent2-key.pem'),
      cert: fixtures.readKey('agent2-cert.pem'),
      ciphers: 'DHE-RSA-AES128-GCM-SHA256',
    });
  });
}

/**
 * OpenSSL security levels reject small keys by policy and can be adjusted with
 * `@SECLEVEL` in the cipher string. BoringSSL does not implement those security
 * levels: the small-key server context is accepted, while the OpenSSL-specific
 * `@SECLEVEL` command is rejected as invalid cipher-string syntax.
 */
function assertOpenSSLSecurityLevelsUnsupported() {
  const options = {
    key: fixtures.readKey('agent11-key.pem'),
    cert: fixtures.readKey('agent11-cert.pem'),
    ciphers: 'DEFAULT',
  };

  tls.createServer(options).close();

  options.ciphers = 'DEFAULT:@SECLEVEL=0';
  assertInvalidCommand(() => tls.createServer(options));
}

/**
 * Node's multi-key tests rely on OpenSSL accepting an array of private keys and
 * matching them with an array of certificates. BoringSSL rejects this mixed
 * EC/RSA identity configuration while configuring the certificate chain, before
 * a client can negotiate either identity.
 */
function assertMultiKeyUnsupported() {
  assert.throws(() => {
    tls.createServer({
      key: [
        fixtures.readKey('ec10-key.pem'),
        fixtures.readKey('agent1-key.pem'),
      ],
      cert: [
        fixtures.readKey('agent1-cert.pem'),
        fixtures.readKey('ec10-cert.pem'),
      ],
    });
  }, {
    code: 'ERR_OSSL_X509_KEY_TYPE_MISMATCH',
    library: 'X.509 certificate routines',
    function: 'OPENSSL_internal',
    reason: 'KEY_TYPE_MISMATCH',
  });
}

/**
 * BoringSSL does not support caller-initiated renegotiation. Even on a TLS 1.2
 * connection, TLSSocket#renegotiate() returns false and the callback receives
 * Node's BoringSSL-specific unsupported-renegotiation error instead of
 * entering the native binding or exercising Node's renegotiation-limit logic.
 */
function testRenegotiationUnsupported() {
  const server = tls.createServer({
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
    maxVersion: 'TLSv1.2',
  }, (socket) => socket.resume());

  server.listen(0, common.mustCall(() => {
    const client = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
      maxVersion: 'TLSv1.2',
    }, common.mustCall(() => {
      const ok = client.renegotiate({}, common.mustCall((err) => {
        assert.throws(() => { throw err; }, {
          code: 'ERR_TLS_RENEGOTIATION_UNSUPPORTED',
          message: 'TLS session renegotiation is unsupported by this TLS ' +
                   'implementation',
        });
        client.destroy();
        server.close();
      }));
      assert.strictEqual(ok, false);
    }));
    client.on('error', common.mustNotCall());
  }));
}

/**
 * OpenSSL exposes the negotiated ephemeral key type, name, and size for TLS
 * clients. With BoringSSL the same ECDHE TLS 1.2 handshake succeeds, but
 * getEphemeralKeyInfo() returns null on the server side and an object whose
 * fields are undefined on the client side.
 */
function testEphemeralKeyInfoUnsupported() {
  const server = tls.createServer({
    key: fixtures.readKey('agent2-key.pem'),
    cert: fixtures.readKey('agent2-cert.pem'),
    ciphers: 'ECDHE-RSA-AES256-GCM-SHA384',
    ecdhCurve: 'prime256v1',
    maxVersion: 'TLSv1.2',
  }, common.mustCall((socket) => {
    assert.strictEqual(socket.getEphemeralKeyInfo(), null);
    socket.end();
  }));

  server.listen(0, common.mustCall(() => {
    const client = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
      maxVersion: 'TLSv1.2',
    }, common.mustCall(() => {
      assert.deepStrictEqual(client.getEphemeralKeyInfo(), {
        type: undefined,
        name: undefined,
        size: undefined,
      });
      server.close();
    }));
  }));
}

/**
 * The protocol matrix tests cover OpenSSL behavior for legacy TLS protocols.
 * For BoringSSL we only need to exhibit that a TLSv1-only client cannot connect
 * to a server whose minimum protocol is TLS 1.2; the client receives the
 * protocol-version alert instead of the OpenSSL version-specific matrix.
 */
function testLegacyProtocolUnsupported() {
  const server = tls.createServer({
    key: fixtures.readKey('agent2-key.pem'),
    cert: fixtures.readKey('agent2-cert.pem'),
    minVersion: 'TLSv1.2',
  }, common.mustNotCall());

  server.on('tlsClientError', common.mustCall());
  server.listen(0, common.mustCall(() => {
    const client = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
      secureProtocol: 'TLSv1_method',
    }, common.mustNotCall());
    client.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_SSL_TLSV1_ALERT_PROTOCOL_VERSION');
      server.close();
    }));
  }));
}

/**
 * BoringSSL can load a multi-PFX option well enough to serve the ECDSA
 * identity, but it does not provide the same OpenSSL multi-identity selection
 * behavior. After the ECDSA handshake succeeds, an RSA-only client fails with
 * no shared cipher instead of selecting the RSA identity from the same PFX list.
 */
function testMultiPfxSelectionDifference() {
  const server = tls.createServer({
    pfx: [
      {
        buf: fixtures.readKey('agent1.pfx'),
        passphrase: 'sample',
      },
      fixtures.readKey('ec.pfx'),
    ],
  }, common.mustCallAtLeast((socket) => socket.end(), 1));

  server.listen(0, common.mustCall(() => {
    const ecdsa = tls.connect(server.address().port, {
      ciphers: 'ECDHE-ECDSA-AES256-GCM-SHA384',
      maxVersion: 'TLSv1.2',
      rejectUnauthorized: false,
    }, common.mustCall(() => {
      assert.strictEqual(ecdsa.getCipher().name,
                         'ECDHE-ECDSA-AES256-GCM-SHA384');
      ecdsa.end();

      server.once('tlsClientError', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ERR_SSL_NO_SHARED_CIPHER');
      }));
      const rsa = tls.connect(server.address().port, {
        ciphers: 'ECDHE-RSA-AES256-GCM-SHA384',
        maxVersion: 'TLSv1.2',
        rejectUnauthorized: false,
      }, common.mustNotCall());
      rsa.on('error', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ERR_SSL_SSLV3_ALERT_HANDSHAKE_FAILURE');
        server.close();
      }));
    }));
  }));
}

/**
 * PSK works for TLS 1.2 in BoringSSL, but Node's PSK tests also cover the
 * default TLS 1.3 path. In that path BoringSSL does not complete a certificate-
 * less PSK-only handshake through Node's current server setup: the server
 * reports NO_CERTIFICATE_SET and the client receives an internal-error alert.
 */
function testPskTls13Unsupported() {
  const key = Buffer.from('d731ef57be09e5204f0b205b60627028', 'hex');
  let gotClientError = false;
  let gotServerError = false;
  function maybeClose(server) {
    if (gotClientError && gotServerError)
      server.close();
  }

  const server = tls.createServer({
    ciphers: 'PSK+HIGH',
    pskCallback() { return key; },
  }, common.mustNotCall());

  server.once('tlsClientError', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_SSL_NO_CERTIFICATE_SET');
    gotServerError = true;
    maybeClose(server);
  }));

  server.listen(0, common.mustCall(() => {
    const client = tls.connect({
      port: server.address().port,
      ciphers: 'PSK+HIGH',
      checkServerIdentity() {},
      pskCallback() {
        return { psk: key, identity: 'TestUser' };
      },
    }, common.mustNotCall());
    client.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_SSL_TLSV1_ALERT_INTERNAL_ERROR');
      gotClientError = true;
      maybeClose(server);
    }));
  }));
}

/**
 * The OpenSSL ticket tests assume that once a TLS 1.3 session is reused, the
 * client will not necessarily receive a replacement session event before close.
 * BoringSSL emits new session tickets on both the initial and resumed TLS 1.3
 * connections, so the resumed connection still emits at least one 'session'
 * event while isSessionReused() is true.
 */
function testTls13SessionTicketSemanticsDiffer() {
  const server = tls.createServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
  }, (socket) => socket.end());

  let session;
  let secondSessionEvents = 0;

  server.listen(0, common.mustCall(() => {
    const first = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
    }, common.mustCall(() => {
      assert.strictEqual(first.isSessionReused(), false);
    }));
    first.on('session', common.mustCallAtLeast((sess) => {
      session = sess;
    }, 1));
    first.on('close', common.mustCall(() => {
      assert(Buffer.isBuffer(session));

      const second = tls.connect({
        port: server.address().port,
        rejectUnauthorized: false,
        session,
      }, common.mustCall(() => {
        assert.strictEqual(second.isSessionReused(), true);
      }));
      second.on('session', common.mustCallAtLeast(() => {
        secondSessionEvents++;
      }, 1));
      second.on('close', common.mustCall(() => {
        assert(secondSessionEvents > 0);
        server.close();
      }));
      second.resume();
    }));
    first.resume();
  }));
}

module.exports = {
  assertFiniteFieldDheUnsupported,
  assertMultiKeyUnsupported,
  assertNoCipherMatch,
  assertOpenSSLSecurityLevelsUnsupported,
  testEphemeralKeyInfoUnsupported,
  testLegacyProtocolUnsupported,
  testMultiPfxSelectionDifference,
  testPskTls13Unsupported,
  testRenegotiationUnsupported,
  testTls13SessionTicketSemanticsDiffer,
};

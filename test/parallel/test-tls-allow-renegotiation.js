'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Tests the allowRenegotiation option of tls.createServer() and tls.connect().

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');

if (isBoringSSL) {
  // BoringSSL servers always refuse renegotiation.
  for (const create of [
    () => tls.createServer({ allowRenegotiation: true }),
    () => new tls.TLSSocket(null, { isServer: true, allowRenegotiation: true }),
  ]) {
    assert.throws(create, { code: 'ERR_TLS_RENEGOTIATION_UNSUPPORTED' });
  }
  require('../common/boringssl').testRenegotiationUnsupported();
  return;
}

// Renegotiation as a protocol feature was dropped after TLS1.2.
tls.DEFAULT_MAX_VERSION = 'TLSv1.2';

const handshakeFailure = hasOpenSSL(4, 0) ?
  'ERR_SSL_TLS_ALERT_HANDSHAKE_FAILURE' : hasOpenSSL(3, 2) ?
    'ERR_SSL_SSL/TLS_ALERT_HANDSHAKE_FAILURE' :
    'ERR_SSL_SSLV3_ALERT_HANDSHAKE_FAILURE';

const idleTimeout = common.platformTimeout(60_000);

const serverOptions = {
  key: fixtures.readKey('rsa_private.pem'),
  cert: fixtures.readKey('rsa_cert.crt'),
};

for (const allowRenegotiation of [null, 0, 'false', {}]) {
  const expected = { code: 'ERR_INVALID_ARG_TYPE' };
  assert.throws(() => tls.createServer({ allowRenegotiation }), expected);
  assert.throws(() => tls.connect({ port: 1, allowRenegotiation }), expected);
}

function connect(server, options, onSecure) {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
    ...options,
  }, () => onSecure(client));
  client.on('close', common.mustCall(() => server.close()));
}

// A side that does not allow renegotiation cannot start one itself.
function assertCannotRenegotiate(socket) {
  assert.strictEqual(socket.renegotiate({}, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_TLS_RENEGOTIATION_DISABLED');
  })), false);
}

// A server that does not allow renegotiation refuses the client's attempt, and
// the attempt leaves the server's side of the connection as it was.
function testServerRefuses({ allowRenegotiation, disableRenegotiation }) {
  const server = tls.createServer({
    ...serverOptions,
    allowRenegotiation,
  }, common.mustCall((socket) => {
    socket.setTimeout(idleTimeout);
    // Has no effect: the attempt is refused without an error on this side.
    if (disableRenegotiation) socket.disableRenegotiation();
    assertCannotRenegotiate(socket);
    socket.on('secure', common.mustNotCall('renegotiation completed'));
    // The client gives up on the connection once its attempt is refused.
    socket.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, handshakeFailure);
      assert.strictEqual(socket.timeout, idleTimeout);
      socket.destroy();
    }));
  }));

  server.listen(0, common.mustCall(() => {
    connect(server, { allowRenegotiation: true }, common.mustCall((client) => {
      client.on('error', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ERR_SSL_NO_RENEGOTIATION');
        client.destroy();
      }));
      client.on('secure', common.mustNotCall('renegotiation completed'));
      assert.strictEqual(client.renegotiate({}), true);
    }));
  }));
}

// A client that does not allow renegotiation refuses the server's request.
function testClientRefuses({ allowRenegotiation }) {
  const server = tls.createServer({
    ...serverOptions,
    allowRenegotiation: true,
  }, common.mustCall((socket) => {
    socket.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_SSL_NO_RENEGOTIATION');
      socket.destroy();
    }));
    socket.on('secure', common.mustNotCall('renegotiation completed'));
    assert.strictEqual(socket.renegotiate({}), true);
  }));

  server.listen(0, common.mustCall(() => {
    connect(server, { allowRenegotiation }, common.mustCall((client) => {
      client.setTimeout(idleTimeout);
      assertCannotRenegotiate(client);
      client.on('secure', common.mustNotCall('renegotiation completed'));
      // The server gives up on the connection once its request is refused.
      client.on('error', common.mustCall((err) => {
        assert.strictEqual(err.code, handshakeFailure);
        assert.strictEqual(client.timeout, idleTimeout);
        client.destroy();
      }));
    }));
  }));
}

function testAllowed() {
  const server = tls.createServer({
    ...serverOptions,
    allowRenegotiation: true,
  }, common.mustCall((socket) => socket.resume()));

  server.listen(0, common.mustCall(() => {
    connect(server, { allowRenegotiation: true }, common.mustCall((client) => {
      assert.strictEqual(client.renegotiate({}, common.mustSucceed(() => {
        client.end();
      })), true);
    }));
  }));
}

// Renegotiation is refused unless allowRenegotiation is true.
for (const allowRenegotiation of [undefined, false]) {
  testServerRefuses({ allowRenegotiation, disableRenegotiation: false });
  testServerRefuses({ allowRenegotiation, disableRenegotiation: true });
  testClientRefuses({ allowRenegotiation });
}
testAllowed();

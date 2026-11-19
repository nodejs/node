'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL, isBoringSSL } = require('../common/crypto');
if (isBoringSSL)
  common.skip('BoringSSL does not support renegotiation');

// An HTTP/2 client refuses a server's request to renegotiate, unless its
// connection was created with allowRenegotiation: true.

const assert = require('assert');
const http2 = require('http2');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const handshakeFailure = hasOpenSSL(4, 0) ?
  'ERR_SSL_TLS_ALERT_HANDSHAKE_FAILURE' : hasOpenSSL(3, 2) ?
    'ERR_SSL_SSL/TLS_ALERT_HANDSHAKE_FAILURE' :
    'ERR_SSL_SSLV3_ALERT_HANDSHAKE_FAILURE';

const tlsOptions = {
  ca: fixtures.readKey('ca1-cert.pem'),
  servername: 'agent1',
  ALPNProtocols: ['h2'],
  maxVersion: 'TLSv1.2',
};

function createServer(onPreface) {
  return tls.createServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
    ALPNProtocols: ['h2'],
    maxVersion: 'TLSv1.2',
    allowRenegotiation: true,
  }, common.mustCall((socket) => {
    // The client's connection preface.
    socket.once('data', () => onPreface(socket));
  }));
}

function testRefused({ createConnection }) {
  const server = createServer(common.mustCall((socket) => {
    socket.on('secure', common.mustNotCall('renegotiation completed'));
    socket.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_SSL_NO_RENEGOTIATION');
      socket.destroy();
    }));
    assert.strictEqual(socket.renegotiate({}), true);
  }));

  server.listen(0, common.mustCall(() => {
    const { port } = server.address();
    const options = createConnection ? {
      createConnection: common.mustCall(() => {
        return tls.connect({ port, ...tlsOptions });
      }),
    } : tlsOptions;
    const session = http2.connect(`https://localhost:${port}`, options);
    session.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, handshakeFailure);
    }));
    session.on('close', common.mustCall(() => server.close()));
  }));
}

function testAllowed() {
  let session;
  const server = createServer(common.mustCall((socket) => {
    assert.strictEqual(socket.renegotiate({}, common.mustSucceed(() => {
      session.destroy();
      socket.destroy();
    })), true);
  }));

  server.listen(0, common.mustCall(() => {
    session = http2.connect(`https://localhost:${server.address().port}`, {
      ...tlsOptions,
      allowRenegotiation: true,
    });
    session.on('close', common.mustCall(() => server.close()));
  }));
}

testRefused({ createConnection: false });
testRefused({ createConnection: true });
testAllowed();

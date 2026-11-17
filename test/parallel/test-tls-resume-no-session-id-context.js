'use strict';

// A server with no session id context of its own - possible when creating a
// TLSSocket directly - can't tell which application a session belongs to. One
// that verifies client certificates mustn't resume any, or a session from an
// application sharing its secure context (and so its ticket keys) would skip
// the client's certificate check. It gets a full handshake instead.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const net = require('node:net');
const tls = require('node:tls');
const fixtures = require('../common/fixtures');

const secureContext = tls.createSecureContext({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
});

// Whether the server resumed each connection it accepted, in order.
const reused = [];

async function listen(requestCert) {
  const server = net.createServer((socket) => {
    const tlsSocket = new tls.TLSSocket(socket, {
      isServer: true,
      secureContext,
      requestCert,
    });
    tlsSocket.on('secure', () => {
      reused.push(tlsSocket.isSessionReused());
      tlsSocket.end();
    });
  });
  await new Promise((resolve) => server.listen(0, resolve));
  return server;
}

// Resolves with the first session issued, once the connection closes. This is
// the raw session from the TLS library: on BoringSSL the 'session' event's
// TLS 1.3 session can't be resumed.
function connect(server, options) {
  return new Promise((resolve) => {
    let session;
    const socket = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
      ...options,
    });
    const { onnewsession } = socket._handle;
    socket._handle.onnewsession = function(id, raw) {
      session ??= raw;
      return onnewsession.call(this, id, raw);
    };
    socket.on('session', () => {});
    socket.on('close', () => resolve(session));
    socket.resume();
  });
}

(async () => {
  const open = await listen(false);
  const verifying = await listen(true);

  for (const maxVersion of ['TLSv1.2', 'TLSv1.3']) {
    reused.length = 0;
    const session = await connect(open, { maxVersion });
    assert.ok(session);
    await connect(open, { maxVersion, session });
    await connect(verifying, { maxVersion, session });
    assert.deepStrictEqual(reused, [false, true, false], maxVersion);
  }

  open.close();
  verifying.close();
})().then(common.mustCall());

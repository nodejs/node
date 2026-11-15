'use strict';

// A resumed session skips authentication, so it may only be resumed for the
// server name it was authenticated for. Otherwise a client trusted by one host
// could resume into another. Asking for a mismatched name gets a full
// handshake, which authenticates the client from scratch like any other.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { isBoringSSL } = require('../common/crypto');
const tls = require('node:tls');
const fixtures = require('../common/fixtures');

// What the server reported for each connection it accepted, in order.
const seen = [];

const server = tls.createServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
}, (socket) => {
  seen.push({ servername: socket.servername, reused: socket.isSessionReused() });
  socket.end();
});

// Connects TLS with options, resolves with the raw issued session when the
// conn closes. We need the raw internal session so we can trick Node into
// (ab)using this for the wrong hostname on resumed connections later.
function connect(options) {
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

// Connecting by IP sends no server name.
const noName = { host: '127.0.0.1' };

server.listen(0, common.mustCall(async () => {
  for (const maxVersion of ['TLSv1.2', 'TLSv1.3']) {
    seen.length = 0;
    const named = await connect({ servername: 'agent1.example', maxVersion });
    const unnamed = await connect({ ...noName, maxVersion });

    // Resuming as the same name, or with no name both times, is fine.
    await connect({ servername: 'agent1.example', maxVersion, session: named });
    await connect({ ...noName, maxVersion, session: unnamed });

    // Asking for a different name gets a full handshake instead.
    await connect({ servername: 'agent3.example', maxVersion, session: named });
    // Going from no SNI to any name gets a full handshake instead.
    await connect({ servername: 'agent1.example', maxVersion, session: unnamed });

    assert.deepStrictEqual(seen, [
      { servername: 'agent1.example', reused: false },
      { servername: false, reused: false },
      { servername: 'agent1.example', reused: true },
      { servername: false, reused: true },
      { servername: 'agent3.example', reused: false },
      { servername: 'agent1.example', reused: false },
    ], maxVersion);
  }

  // BoringSSL doesn't support TLS 1.3 external PSK.
  if (!isBoringSSL) {
    // Make sure fresh PSK & PSK resumption work correctly too - this is risky as
    // TLS1.3 PSK works by pretending to be resumption (but it's not).
    const secret = Buffer.alloc(32, 1);
    const ciphers = 'TLS_AES_128_GCM_SHA256';
    // OpenSSL offers resumption tickets to pskCallback as identities too (cut
    // at their first NUL byte). Answering those would turn a ticket into an
    // external PSK whose binder can't verify, so only answer for 'client'.
    const pskCallback = (socket, identity) => (identity === 'client' ? secret : undefined);
    const psk = tls.createServer({ ciphers, pskCallback },
                                 common.mustCall((socket) => {
                                   assert.strictEqual(socket.servername, 'psk.example');
                                   socket.end();
                                 }, 2));
    await new Promise((resolve) => psk.listen(0, resolve));
    const pskOptions = {
      port: psk.address().port,
      servername: 'psk.example',
      ciphers,
      pskCallback: () => ({ psk: secret, identity: 'client' }),
    };
    const ticket = await connect(pskOptions);
    assert.ok(ticket);
    await connect({ ...pskOptions, session: ticket });
    psk.close();
  }
  server.close();
}));
